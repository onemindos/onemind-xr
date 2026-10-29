// ingestd-proof: plain Go collectors on nats.go for the OneMind ingestion proof (M0).
//
// One binary runs eleven sources (one per delivery mode). Each source:
//   - is owned by exactly one instance: a lease in KV "ingest-locks" (bucket TTL 10 s);
//   - publishes to JetStream synchronously, waits for the PubAck, retries with backoff,
//     and sets Nats-Msg-Id so replays are dropped by the stream;
//   - validates records against a JSON Schema; failures go to evt.feed.{source}.rejected;
//   - checkpoints its cursor/ETag/Last-Modified/seen-set in KV "ingest-state" only AFTER
//     every record of a fetch is acked, so a crash never skips data and a restart does
//     not republish beyond the stream's duplicate window;
//   - honours a per-source rate limit, If-None-Match/If-Modified-Since, and 429 Retry-After;
//   - reports tel.feed.{source}.status every 10 s (fetchedAt, count, lastError, stale).
//
// Subjects follow the locked omos grammar. Synthetic upstreams only (stubs/upstream_stub.py).
package main

import (
	"bufio"
	"context"
	"encoding/json"
	"encoding/xml"
	"errors"
	"flag"
	"fmt"
	"io"
	"log/slog"
	"net/http"
	"os"
	"os/signal"
	"regexp"
	"strconv"
	"strings"
	"sync"
	"sync/atomic"
	"syscall"
	"time"

	"github.com/coder/websocket"
	mqtt "github.com/eclipse/paho.mqtt.golang"
	"github.com/nats-io/nats.go"
	"github.com/nats-io/nats.go/jetstream"
	"github.com/santhosh-tekuri/jsonschema/v6"
	"golang.org/x/time/rate"
)

const stub = "http://127.0.0.1:38080"

var (
	log      = slog.New(slog.NewJSONHandler(os.Stdout, nil))
	instance string
)

// ---------------------------------------------------------------- collector core

type Collector struct {
	nc     *nats.Conn
	js     jetstream.JetStream
	state  jetstream.KeyValue
	locks  jetstream.KeyValue
	video  jetstream.KeyValue
	mu     sync.Mutex
	status map[string]*Status
	m      metrics
}

type metrics struct {
	published, duplicates, rejected, retries, fetchErrors, dropped atomic.Int64
}

type Status struct {
	Source     string     `json:"source"`
	Engine     string     `json:"engine"`
	Instance   string     `json:"instance"`
	FetchedAt  *time.Time `json:"fetchedAt"`
	Count      int        `json:"count"`
	LastError  *string    `json:"lastError"`
	ErrorAt    *time.Time `json:"errorAt"`
	StaleAfter float64    `json:"staleAfterSec"`
	AgeSec     *float64   `json:"ageSec"`
	Stale      bool       `json:"stale"`
	Leader     bool       `json:"leader"`
}

func (c *Collector) ok(src string, n int) {
	c.mu.Lock()
	defer c.mu.Unlock()
	now := time.Now()
	s := c.status[src]
	s.FetchedAt, s.Count, s.LastError = &now, n, nil
}

func (c *Collector) fail(src string, err error) {
	c.m.fetchErrors.Add(1)
	c.mu.Lock()
	defer c.mu.Unlock()
	now := time.Now()
	e := err.Error()
	c.status[src].LastError, c.status[src].ErrorAt = &e, &now
	log.Warn("fetch failed", "source", src, "err", e)
}

// publish blocks until JetStream acks (or ctx ends). Never drops.
func (c *Collector) publish(ctx context.Context, subj, id string, v any) error {
	data, err := json.Marshal(v)
	if err != nil {
		return err
	}
	backoff := 250 * time.Millisecond
	for {
		msg := nats.NewMsg(subj)
		msg.Header.Set(jetstream.MsgIDHeader, id)
		msg.Data = data
		pctx, cancel := context.WithTimeout(ctx, 5*time.Second)
		ack, err := c.js.PublishMsg(pctx, msg)
		cancel()
		if err == nil {
			if ack.Duplicate {
				c.m.duplicates.Add(1)
			} else {
				c.m.published.Add(1)
			}
			return nil
		}
		if ctx.Err() != nil {
			return ctx.Err()
		}
		c.m.retries.Add(1)
		log.Warn("publish retry", "subject", subj, "err", err.Error(), "backoff", backoff.String())
		select {
		case <-time.After(backoff):
		case <-ctx.Done():
			return ctx.Err()
		}
		backoff = min(backoff*2, 10*time.Second)
	}
}

func (c *Collector) reject(ctx context.Context, src, reason string, raw []byte) error {
	c.m.rejected.Add(1)
	r := string(raw)
	if len(r) > 2000 {
		r = r[:2000]
	}
	return c.publish(ctx, "evt.feed."+src+".rejected", fmt.Sprintf("rej:%s:%x:%d", src, fnv(raw), time.Now().UnixNano()), // unique per occurrence
		map[string]any{"source": src, "reason": reason, "raw": r, "at": time.Now()})
}

func fnv(b []byte) uint64 {
	h := uint64(14695981039346656037)
	for _, x := range b {
		h ^= uint64(x)
		h *= 1099511628211
	}
	return h
}

// ---------------------------------------------------------------- checkpoint state (KV, written after acks)

type State struct {
	ETag    string   `json:"etag,omitempty"`
	LastMod string   `json:"lastModified,omitempty"`
	Cursor  string   `json:"cursor,omitempty"`
	Seen    []string `json:"seen,omitempty"` // bounded ring of recent ids
}

// load never treats "could not read" as "no state": that would republish everything the
// upstream still lists (measured in chaos run 4). Only a missing key means a fresh start.
func (c *Collector) load(ctx context.Context, src string) (State, error) {
	var s State
	e, err := c.state.Get(ctx, src)
	if errors.Is(err, jetstream.ErrKeyNotFound) {
		return s, nil
	}
	if err != nil {
		return s, fmt.Errorf("checkpoint read: %w", err)
	}
	return s, json.Unmarshal(e.Value(), &s)
}

func (c *Collector) loadOrWait(ctx context.Context, src string) (State, bool) {
	st, err := c.load(ctx, src)
	if err != nil {
		c.fail(src, err)
		select {
		case <-time.After(2 * time.Second):
		case <-ctx.Done():
		}
		return st, false
	}
	return st, true
}

func (c *Collector) save(ctx context.Context, src string, s State) {
	if len(s.Seen) > 2000 {
		s.Seen = s.Seen[len(s.Seen)-2000:]
	}
	b, _ := json.Marshal(s)
	if _, err := c.state.Put(ctx, src, b); err != nil {
		log.Warn("checkpoint failed (will refetch; Nats-Msg-Id absorbs it)", "source", src, "err", err.Error())
	}
}

// ---------------------------------------------------------------- single owner per source (KV lease)

func (c *Collector) owned(ctx context.Context, src string, run func(context.Context)) {
	for ctx.Err() == nil {
		rev, err := c.locks.Create(ctx, src, []byte(instance))
		if err != nil {
			select {
			case <-time.After(2 * time.Second):
			case <-ctx.Done():
			}
			continue
		}
		log.Info("lease acquired", "source", src, "instance", instance)
		c.setLeader(src, true)
		lctx, cancel := context.WithCancel(ctx)
		done := make(chan struct{})
		go func() { run(lctx); close(done) }()
		t := time.NewTicker(3 * time.Second)
	renew:
		for {
			select {
			case <-t.C:
				r, err := c.locks.Update(ctx, src, []byte(instance), rev)
				if err == nil {
					rev = r
					continue
				}
				// Bus unreachable is not ownership lost: nobody else can take the lease
				// while the bus is down, and our publishes wait for acks anyway. Keep
				// running (and buffering) until the lease is seen held by someone else.
				if c.nc.Status() != nats.CONNECTED || errors.Is(err, context.DeadlineExceeded) {
					log.Warn("lease renew deferred (bus unreachable)", "source", src, "err", err.Error())
					continue
				}
				if e, gerr := c.locks.Get(ctx, src); gerr == nil && string(e.Value()) == instance {
					rev = e.Revision()
					continue
				} else if errors.Is(gerr, jetstream.ErrKeyNotFound) {
					if r2, cerr := c.locks.Create(ctx, src, []byte(instance)); cerr == nil {
						rev = r2 // lease expired while the bus was down; nobody took it
						log.Info("lease re-created after outage", "source", src)
						continue
					}
				}
				log.Warn("lease lost to another instance", "source", src, "err", err.Error())
				break renew
			case <-done:
				break renew
			case <-ctx.Done():
				break renew
			}
		}
		t.Stop()
		cancel()
		<-done
		c.setLeader(src, false)
		if ctx.Err() != nil {
			_ = c.locks.Delete(context.Background(), src, jetstream.LastRevision(rev))
		}
	}
}

func (c *Collector) setLeader(src string, v bool) {
	c.mu.Lock()
	c.status[src].Leader = v
	c.mu.Unlock()
}

// ---------------------------------------------------------------- schemas

func schema(s string) *jsonschema.Schema {
	doc, err := jsonschema.UnmarshalJSON(strings.NewReader(s))
	if err != nil {
		panic(err)
	}
	comp := jsonschema.NewCompiler()
	if err := comp.AddResource("s.json", doc); err != nil {
		panic(err)
	}
	return comp.MustCompile("s.json")
}

func validate(sch *jsonschema.Schema, raw []byte) (map[string]any, error) {
	inst, err := jsonschema.UnmarshalJSON(strings.NewReader(string(raw)))
	if err != nil {
		return nil, err
	}
	if err := sch.Validate(inst); err != nil {
		return nil, err
	}
	var m map[string]any
	err = json.Unmarshal(raw, &m)
	return m, err
}

// ---------------------------------------------------------------- HTTP polling helper

var httpc = &http.Client{Timeout: 20 * time.Second}

type fetchResult struct {
	code    int
	body    []byte
	etag    string
	lastMod string
}

// fetch does one polite request: rate limit, conditional headers, Retry-After on 429/503.
func fetch(ctx context.Context, lim *rate.Limiter, method, url string, st State) (fetchResult, error) {
	backoff := time.Second
	for attempt := 0; ; attempt++ {
		if err := lim.Wait(ctx); err != nil {
			return fetchResult{}, err
		}
		req, _ := http.NewRequestWithContext(ctx, method, url, nil)
		req.Header.Set("User-Agent", "onemind-ingest-proof/0")
		if st.ETag != "" {
			req.Header.Set("If-None-Match", st.ETag)
		}
		if st.LastMod != "" {
			req.Header.Set("If-Modified-Since", st.LastMod)
		}
		resp, err := httpc.Do(req)
		if err != nil {
			return fetchResult{}, err
		}
		body, _ := io.ReadAll(io.LimitReader(resp.Body, 32<<20))
		resp.Body.Close()
		if (resp.StatusCode == 429 || resp.StatusCode == 503) && attempt < 3 {
			wait := backoff
			if ra, err := strconv.Atoi(resp.Header.Get("Retry-After")); err == nil {
				wait = time.Duration(ra) * time.Second
			}
			select {
			case <-time.After(wait):
			case <-ctx.Done():
				return fetchResult{}, ctx.Err()
			}
			backoff *= 2
			continue
		}
		if resp.StatusCode != 200 && resp.StatusCode != 304 {
			return fetchResult{}, fmt.Errorf("http %d", resp.StatusCode)
		}
		return fetchResult{resp.StatusCode, body, resp.Header.Get("ETag"), resp.Header.Get("Last-Modified")}, nil
	}
}

type rec struct {
	subject, id string
	raw         []byte
}

// emitAll validates, skips seen ids, publishes, and returns the ids it acked.
func (c *Collector) emitAll(ctx context.Context, src string, sch *jsonschema.Schema, recs []rec, st *State) error {
	seen := make(map[string]bool, len(st.Seen))
	for _, s := range st.Seen {
		seen[s] = true
	}
	for _, r := range recs {
		m, err := validate(sch, r.raw)
		if err != nil {
			if err := c.reject(ctx, src, err.Error(), r.raw); err != nil {
				return err
			}
			continue
		}
		if seen[r.id] {
			continue
		}
		m["source"], m["fetchedAt"] = src, time.Now()
		if err := c.publish(ctx, r.subject, r.id, m); err != nil {
			return err
		}
		st.Seen = append(st.Seen, r.id)
		seen[r.id] = true
	}
	return nil
}

func (c *Collector) poll(ctx context.Context, src, url string, every time.Duration, lim *rate.Limiter,
	sch *jsonschema.Schema, parse func([]byte) ([]rec, error)) {
	t := time.NewTicker(every)
	defer t.Stop()
	for {
		st, ok := c.loadOrWait(ctx, src)
		if !ok {
			if ctx.Err() != nil {
				return
			}
			continue
		}
		res, err := fetch(ctx, lim, "GET", url, st)
		switch {
		case err != nil:
			if ctx.Err() == nil {
				c.fail(src, err)
			}
		case res.code == 304:
			c.ok(src, 0)
		default:
			recs, perr := parse(res.body)
			if perr != nil {
				_ = c.reject(ctx, src, "parse: "+perr.Error(), res.body)
				c.fail(src, perr)
				break
			}
			if err := c.emitAll(ctx, src, sch, recs, &st); err != nil {
				break // not checkpointed: next poll refetches, dedup absorbs it
			}
			st.ETag, st.LastMod = res.etag, res.lastMod
			c.save(ctx, src, st)
			c.ok(src, len(recs))
		}
		select {
		case <-t.C:
		case <-ctx.Done():
			return
		}
	}
}

// ---------------------------------------------------------------- the eleven sources

func (c *Collector) usgs(ctx context.Context) {
	sch := schema(`{"type":"object","required":["id","mag","time","lon","lat"],"properties":{"id":{"type":"string"},"mag":{"type":"number"},"time":{"type":"integer"}}}`)
	c.poll(ctx, "usgs", stub+"/usgs.geojson", 5*time.Second, rate.NewLimiter(rate.Every(5*time.Second), 1), sch,
		func(b []byte) ([]rec, error) {
			var fc struct {
				Features []struct {
					ID         string `json:"id"`
					Properties map[string]any
					Geometry   *struct{ Coordinates []float64 }
				}
			}
			if err := json.Unmarshal(b, &fc); err != nil {
				return nil, err
			}
			var out []rec
			for _, f := range fc.Features {
				m := map[string]any{"id": f.ID, "mag": f.Properties["mag"], "time": f.Properties["time"], "place": f.Properties["place"]}
				if f.Geometry != nil && len(f.Geometry.Coordinates) >= 2 {
					m["lon"], m["lat"] = f.Geometry.Coordinates[0], f.Geometry.Coordinates[1]
				}
				raw, _ := json.Marshal(m)
				out = append(out, rec{"intel.seismic.usgs.event", "usgs:" + f.ID, raw})
			}
			return out, nil
		})
}

func (c *Collector) nws(ctx context.Context) {
	src := "nws"
	sch := schema(`{"type":"object","required":["id","event","sent"],"properties":{"id":{"type":"string"},"sent":{"type":"integer"}}}`)
	lim := rate.NewLimiter(rate.Every(500*time.Millisecond), 1)
	for ctx.Err() == nil {
		st, ok := c.loadOrWait(ctx, src)
		if !ok {
			continue
		}
		cur := st.Cursor
		if cur == "" {
			cur = "-1"
		}
		res, err := fetch(ctx, lim, "GET", stub+"/nws/longpoll?cursor="+cur, State{})
		if err != nil {
			if ctx.Err() == nil {
				c.fail(src, err)
				time.Sleep(time.Second)
			}
			continue
		}
		var lp struct {
			Cursor int               `json:"cursor"`
			Alerts []json.RawMessage `json:"alerts"`
		}
		if err := json.Unmarshal(res.body, &lp); err != nil {
			_ = c.reject(ctx, src, err.Error(), res.body)
			continue
		}
		var recs []rec
		for _, a := range lp.Alerts {
			var id struct{ ID string }
			_ = json.Unmarshal(a, &id)
			recs = append(recs, rec{"intel.weather.nws.alert", "nws:" + id.ID, a})
		}
		if err := c.emitAll(ctx, src, sch, recs, &st); err != nil {
			continue
		}
		st.Cursor = strconv.Itoa(lp.Cursor) // checkpoint only after acks
		c.save(ctx, src, st)
		c.ok(src, len(recs))
	}
}

func (c *Collector) eonet(ctx context.Context) {
	src := "eonet"
	sch := schema(`{"type":"object","required":["id","lon","lat"],"properties":{"id":{"type":"string"}}}`)
	for ctx.Err() == nil {
		st, ok := c.loadOrWait(ctx, src)
		if !ok {
			continue
		}
		req, _ := http.NewRequestWithContext(ctx, "GET", stub+"/eonet/sse", nil)
		req.Header.Set("Accept", "text/event-stream")
		if st.Cursor != "" {
			req.Header.Set("Last-Event-ID", st.Cursor) // resume: no gap across reconnects
		}
		resp, err := (&http.Client{}).Do(req)
		if err != nil {
			c.fail(src, err)
			time.Sleep(2 * time.Second)
			continue
		}
		sc := bufio.NewScanner(resp.Body)
		var evID string
		for sc.Scan() {
			line := sc.Text()
			switch {
			case strings.HasPrefix(line, "id: "):
				evID = strings.TrimPrefix(line, "id: ")
			case strings.HasPrefix(line, "data: "):
				raw := []byte(strings.TrimPrefix(line, "data: "))
				var id struct{ ID string }
				_ = json.Unmarshal(raw, &id)
				if err := c.emitAll(ctx, src, sch, []rec{{"intel.natural.eonet.event", "eonet:" + id.ID, raw}}, &st); err != nil {
					break
				}
				st.Cursor = evID
				c.save(ctx, src, st)
				c.ok(src, 1)
			}
		}
		resp.Body.Close()
		if ctx.Err() == nil {
			c.fail(src, errors.New("sse stream ended"))
			time.Sleep(time.Second)
		}
	}
}

func (c *Collector) ais(ctx context.Context) {
	src := "ais"
	sch := schema(`{"type":"object","required":["mmsi","ts","lat","lon"],"properties":{"mmsi":{"type":"integer"},"ts":{"type":"integer"},"lat":{"type":"number","minimum":-90,"maximum":90},"lon":{"type":"number","minimum":-180,"maximum":180}}}`)
	// The reader never blocks on the bus: it keeps reading (and answering pings) while a
	// publisher drains a bounded queue. If the bus is down longer than the queue covers
	// (20000 msgs = 400 s at 50 msg/s) the oldest positions are dropped and counted.
	q := make(chan []byte, 20000)
	go func() {
		for {
			select {
			case raw := <-q:
				m, err := validate(sch, raw)
				if err != nil {
					_ = c.reject(ctx, src, err.Error(), raw)
					continue
				}
				var p struct { // decode ids as integers: never format them from float64
					MMSI int64 `json:"mmsi"`
					TS   int64 `json:"ts"`
				}
				_ = json.Unmarshal(raw, &p)
				m["source"], m["fetchedAt"] = src, time.Now()
				if err := c.publish(ctx, fmt.Sprintf("evt.world.sea.%d", p.MMSI), fmt.Sprintf("ais:%d:%d", p.MMSI, p.TS), m); err != nil {
					return
				}
				c.ok(src, 1)
			case <-ctx.Done():
				return
			}
		}
	}()
	for ctx.Err() == nil {
		conn, _, err := websocket.Dial(ctx, "ws://127.0.0.1:38081/ais", nil)
		if err != nil {
			c.fail(src, err)
			time.Sleep(2 * time.Second)
			continue
		}
		conn.SetReadLimit(1 << 20)
		for {
			_, raw, err := conn.Read(ctx)
			if err != nil {
				if ctx.Err() == nil {
					c.fail(src, err)
				}
				break
			}
			select {
			case q <- raw:
			default:
				<-q // full: drop the oldest position, keep the newest
				c.m.dropped.Add(1)
				q <- raw
			}
		}
		conn.CloseNow()
		time.Sleep(time.Second)
	}
}

func (c *Collector) cap(ctx context.Context) {
	src := "cap"
	sch := schema(`{"type":"object","required":["identifier","event","sent"],"properties":{"identifier":{"type":"string"}}}`)
	mux := http.NewServeMux()
	mux.HandleFunc("POST /webhook/cap", func(w http.ResponseWriter, r *http.Request) {
		raw, _ := io.ReadAll(io.LimitReader(r.Body, 1<<20))
		m, err := validate(sch, raw)
		if err != nil {
			_ = c.reject(r.Context(), src, err.Error(), raw)
			http.Error(w, "rejected: "+err.Error(), http.StatusUnprocessableEntity)
			return
		}
		// Hold the response until the JetStream ack (up to 4.5 s, under typical sender
		// timeouts). Failing fast burned the sender's retry budget in chaos runs 2-4.
		m["source"], m["fetchedAt"] = src, time.Now()
		pctx, cancel := context.WithTimeout(r.Context(), 4500*time.Millisecond)
		defer cancel()
		if err := c.publish(pctx, "intel.alert.cap.alert", "cap:"+m["identifier"].(string), m); err != nil {
			http.Error(w, "bus unavailable", http.StatusServiceUnavailable) // sender retries
			return
		}
		c.ok(src, 1)
		w.WriteHeader(http.StatusOK)
	})
	srv := &http.Server{Addr: "127.0.0.1:38090", Handler: mux}
	go func() { <-ctx.Done(); _ = srv.Close() }()
	if err := srv.ListenAndServe(); err != nil && ctx.Err() == nil {
		c.fail(src, err)
	}
}

func (c *Collector) mqttSrc(ctx context.Context) {
	src := "mqtt"
	sch := schema(`{"type":"object","required":["station","seq","tempC"],"properties":{"seq":{"type":"integer"},"tempC":{"type":"number"}}}`)
	opts := mqtt.NewClientOptions().AddBroker("tcp://127.0.0.1:31883").SetClientID("onemind-ingest-ws-go").
		SetCleanSession(false).SetAutoReconnect(true).SetOrderMatters(true).SetAutoAckDisabled(true)
	opts.SetOnConnectHandler(func(cl mqtt.Client) {
		cl.Subscribe("sensors/+/reading", 1, func(_ mqtt.Client, msg mqtt.Message) {
			raw := msg.Payload()
			m, err := validate(sch, raw)
			if err != nil {
				if c.reject(ctx, src, err.Error(), raw) == nil {
					msg.Ack()
				}
				return
			}
			var p struct {
				Station string `json:"station"`
				Seq     int64  `json:"seq"`
			}
			_ = json.Unmarshal(raw, &p)
			m["source"], m["fetchedAt"] = src, time.Now()
			if err := c.publish(ctx, "tel."+p.Station+".weather.reading", fmt.Sprintf("mqtt:%s:%d", p.Station, p.Seq), m); err != nil {
				return // no PUBACK: the broker redelivers
			}
			msg.Ack() // PUBACK only after the JetStream ack
			c.ok(src, 1)
		})
	})
	cl := mqtt.NewClient(opts)
	for ctx.Err() == nil {
		if t := cl.Connect(); t.Wait() && t.Error() == nil {
			break
		}
		time.Sleep(2 * time.Second)
	}
	<-ctx.Done()
	cl.Disconnect(500)
}

func (c *Collector) celestrak(ctx context.Context) {
	sch := schema(`{"type":"object","required":["name","l1","l2"],"properties":{"l1":{"type":"string","pattern":"^1 "},"l2":{"type":"string","pattern":"^2 "}}}`)
	c.poll(ctx, "celestrak", stub+"/celestrak/stations.txt", 30*time.Second, rate.NewLimiter(rate.Every(30*time.Second), 1), sch,
		func(b []byte) ([]rec, error) {
			lines := strings.Split(strings.TrimSpace(string(b)), "\n")
			var out []rec
			for i := 0; i+2 < len(lines); i += 3 {
				l1 := lines[i+1]
				norad, epoch := "", ""
				if len(l1) >= 32 {
					norad, epoch = l1[2:7], l1[18:32]
				}
				raw, _ := json.Marshal(map[string]any{"name": strings.TrimSpace(lines[i]), "l1": l1, "l2": lines[i+2], "norad": norad, "epoch": epoch})
				out = append(out, rec{"intel.space.celestrak.tle", "tle:" + norad + ":" + epoch, raw})
			}
			return out, nil
		})
}

func (c *Collector) gdacs(ctx context.Context) {
	sch := schema(`{"type":"object","required":["id","title"]}`)
	c.poll(ctx, "gdacs", stub+"/gdacs/rss.xml", 10*time.Second, rate.NewLimiter(rate.Every(10*time.Second), 1), sch,
		func(b []byte) ([]rec, error) {
			var rss struct {
				Items []struct {
					GUID  string  `xml:"guid"`
					Title string  `xml:"title"`
					Lat   float64 `xml:"http://www.w3.org/2003/01/geo/wgs84_pos# lat"`
					Lon   float64 `xml:"http://www.w3.org/2003/01/geo/wgs84_pos# long"`
				} `xml:"channel>item"`
			}
			if err := xml.Unmarshal(b, &rss); err != nil {
				return nil, err
			}
			var out []rec
			for _, it := range rss.Items {
				raw, _ := json.Marshal(map[string]any{"id": it.GUID, "title": it.Title, "lat": it.Lat, "lon": it.Lon})
				out = append(out, rec{"intel.disaster.gdacs.event", "gdacs:" + it.GUID, raw})
			}
			return out, nil
		})
}

var rowRe = regexp.MustCompile(`<tr data-id="([^"]+)"><td class="name">([^<]+)</td><td class="cust">([0-9]+)</td></tr>`)

func (c *Collector) outages(ctx context.Context) {
	sch := schema(`{"type":"object","required":["id","name","customers"],"properties":{"customers":{"type":"integer"}}}`)
	c.poll(ctx, "outages", stub+"/outages.html", 15*time.Second, rate.NewLimiter(rate.Every(15*time.Second), 1), sch,
		func(b []byte) ([]rec, error) {
			var out []rec
			for _, m := range rowRe.FindAllStringSubmatch(string(b), -1) {
				n, _ := strconv.Atoi(m[3])
				raw, _ := json.Marshal(map[string]any{"id": m[1], "name": m[2], "customers": n})
				out = append(out, rec{"intel.infra.outages.event", "outages:" + m[1] + ":" + m[3], raw})
			}
			return out, nil
		})
}

func (c *Collector) tiles(ctx context.Context) {
	lim := rate.NewLimiter(rate.Every(30*time.Second), 1)
	for ctx.Err() == nil {
		if _, err := fetch(ctx, lim, "HEAD", stub+"/tiles/3/2/3.png", State{}); err != nil {
			if ctx.Err() == nil {
				c.fail("tiles", err)
			}
		} else {
			c.ok("tiles", 1)
		}
	}
}

func (c *Collector) mediamtx(ctx context.Context) {
	lim := rate.NewLimiter(rate.Every(10*time.Second), 1)
	for ctx.Err() == nil {
		res, err := fetch(ctx, lim, "GET", "http://127.0.0.1:39997/v3/paths/list", State{})
		if err != nil {
			if ctx.Err() == nil {
				c.fail("mediamtx", err)
			}
			continue
		}
		var pl struct {
			Items []struct {
				Name   string `json:"name"`
				Ready  bool   `json:"ready"`
				Tracks []string
			} `json:"items"`
		}
		if err := json.Unmarshal(res.body, &pl); err != nil {
			c.fail("mediamtx", err)
			continue
		}
		for _, p := range pl.Items {
			b, _ := json.Marshal(map[string]any{"path": p.Name, "ready": p.Ready, "tracks": p.Tracks,
				"rtsp": "rtsp://127.0.0.1:38554/" + p.Name, "seenAt": time.Now(), "engine": "go"})
			if _, err := c.video.Put(ctx, p.Name, b); err != nil {
				c.fail("mediamtx", err)
			}
		}
		c.ok("mediamtx", len(pl.Items))
	}
}

// ---------------------------------------------------------------- health + metrics

func (c *Collector) heartbeat(ctx context.Context) {
	t := time.NewTicker(10 * time.Second)
	defer t.Stop()
	for {
		select {
		case <-t.C:
		case <-ctx.Done():
			return
		}
		c.mu.Lock()
		var snap []Status
		for _, s := range c.status {
			cp := *s
			if cp.FetchedAt != nil {
				age := time.Since(*cp.FetchedAt).Seconds()
				cp.AgeSec = &age
				cp.Stale = age > cp.StaleAfter
			} else {
				cp.Stale = true
			}
			snap = append(snap, cp)
		}
		c.mu.Unlock()
		for _, s := range snap {
			if !s.Leader {
				continue // only the owner reports a source
			}
			b, _ := json.Marshal(s)
			// core publish into the TEL stream: a missed heartbeat is not data loss
			_ = c.nc.Publish("tel.feed."+s.Source+".status", b)
		}
	}
}

func (c *Collector) serveMetrics(addr string) {
	http.HandleFunc("/metrics", func(w http.ResponseWriter, _ *http.Request) {
		fmt.Fprintf(w, "ingest_published_total %d\ningest_duplicates_total %d\ningest_rejected_total %d\ningest_publish_retries_total %d\ningest_fetch_errors_total %d\ningest_dropped_total %d\n",
			c.m.published.Load(), c.m.duplicates.Load(), c.m.rejected.Load(), c.m.retries.Load(), c.m.fetchErrors.Load(), c.m.dropped.Load())
		c.mu.Lock()
		defer c.mu.Unlock()
		for _, s := range c.status {
			age := -1.0
			if s.FetchedAt != nil {
				age = time.Since(*s.FetchedAt).Seconds()
			}
			fmt.Fprintf(w, "ingest_feed_age_seconds{source=%q} %f\ningest_feed_leader{source=%q,instance=%q} %d\n", s.Source, age, s.Source, instance, b2i(s.Leader))
		}
	})
	_ = http.ListenAndServe(addr, nil)
}

func b2i(b bool) int {
	if b {
		return 1
	}
	return 0
}

// ---------------------------------------------------------------- main

func main() {
	url := flag.String("nats", "nats://127.0.0.1:34222", "NATS URL")
	flag.StringVar(&instance, "instance", "a", "instance id (for the lease)")
	maddr := flag.String("metrics", "127.0.0.1:38196", "metrics listen address")
	only := flag.String("only", "", "comma-separated sources to run (default all)")
	flag.Parse()

	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer stop()

	// RetryOnFailedConnect + infinite reconnects: start and keep running while NATS is down.
	nc, err := nats.Connect(*url, nats.Name("ingestd-proof-"+instance), nats.RetryOnFailedConnect(true),
		nats.MaxReconnects(-1), nats.ReconnectWait(time.Second),
		nats.DisconnectErrHandler(func(_ *nats.Conn, err error) { log.Warn("nats disconnected", "err", fmt.Sprint(err)) }),
		nats.ReconnectHandler(func(_ *nats.Conn) { log.Info("nats reconnected") }))
	if err != nil {
		log.Error("connect", "err", err.Error())
		os.Exit(1)
	}
	js, _ := jetstream.New(nc)
	c := &Collector{nc: nc, js: js, status: map[string]*Status{}}
	for c.state == nil || c.locks == nil || c.video == nil {
		if c.state, err = js.KeyValue(ctx, "ingest-state"); err == nil {
			c.locks, _ = js.KeyValue(ctx, "ingest-locks")
			c.video, _ = js.KeyValue(ctx, "overwatch-video")
		}
		if c.locks == nil || c.video == nil {
			c.state = nil
			if ctx.Err() != nil {
				return
			}
			time.Sleep(time.Second)
		}
	}
	sources := []struct {
		name  string
		stale float64
		run   func(context.Context)
	}{
		{"usgs", 60, c.usgs}, {"nws", 60, c.nws}, {"eonet", 30, c.eonet}, {"ais", 15, c.ais},
		{"cap", 30, c.cap}, {"mqtt", 15, c.mqttSrc}, {"celestrak", 180, c.celestrak}, {"gdacs", 60, c.gdacs},
		{"outages", 60, c.outages}, {"tiles", 120, c.tiles}, {"mediamtx", 60, c.mediamtx},
	}
	var wg sync.WaitGroup
	for _, s := range sources {
		if *only != "" && !strings.Contains(","+*only+",", ","+s.name+",") {
			continue
		}
		c.status[s.name] = &Status{Source: s.name, Engine: "go", Instance: instance, StaleAfter: s.stale}
		wg.Add(1)
		go func() { defer wg.Done(); c.owned(ctx, s.name, s.run) }()
	}
	go c.heartbeat(ctx)
	go c.serveMetrics(*maddr)
	log.Info("ingestd-proof up", "instance", instance)
	wg.Wait()
	_ = nc.Drain()
}
