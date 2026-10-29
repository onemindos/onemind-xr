// End-to-end integration run for OneMind Phase 2: every service built from its branch tip,
// one disposable nats-server with the A19 accounts (feat/nats-identities) and the stream CRs
// (feat/stream-caps, feat/entity-service, feat/ingest-framework), the scripted drone sim-x1.
// Writes transcript.jsonl, results.json and per-process logs to the evidence dir, then holds
// the stack (stack.json in the run dir) until RUN/stop exists, for the browser step.
package main

import (
	"crypto/ed25519"
	"crypto/rand"
	"crypto/x509"
	"encoding/base64"
	"encoding/hex"
	"encoding/json"
	"encoding/pem"
	"fmt"
	"math"
	"net"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"sort"
	"strings"
	"sync"
	"time"

	"github.com/nats-io/nats.go"
)

var (
	RUN  = "/Users/danielcarku/Desktop/work/.run/tmp-e2e/run"
	EV   = envOr("E2E_EV", "/Users/danielcarku/Desktop/work/onemind-xr/docs/evidence/INT/E2E")
	DEV  = os.Getenv("E2E_DEV") == "1"
	BIN  = "/Users/danielcarku/Desktop/work/.run/tmp-e2e/bin"
	SRC  = "/Users/danielcarku/Desktop/work/.run/tmp-e2e/src"
	PY   = "/Users/danielcarku/Desktop/work/.tools/py/bin/python"
	SIM  = "/Users/danielcarku/Desktop/work/onemind-xr/scripts/sim/mavlink_sim_drone.py"
	NATS = "/Users/danielcarku/Desktop/work/.tools/bin/nats-server"
	HOME = [2]float64{30.2672, -97.7431} // lat, lon
)

// ---------- recording ----------

type rec struct {
	At      time.Time
	Subject string
	Data    map[string]any
	Raw     []byte
	Hdr     nats.Header
}

type recorder struct {
	mu   sync.Mutex
	msgs []rec
	f    *os.File
	tel  int
	ent  int
}

func (r *recorder) add(m *nats.Msg) {
	var d map[string]any
	_ = json.Unmarshal(m.Data, &d)
	r.mu.Lock()
	defer r.mu.Unlock()
	r.msgs = append(r.msgs, rec{time.Now(), m.Subject, d, append([]byte(nil), m.Data...), m.Header})
	log := true
	if strings.HasPrefix(m.Subject, "tel.fleet.") {
		r.tel++
		log = r.tel%20 == 1
	}
	if strings.HasPrefix(m.Subject, "ent.") {
		r.ent++
		log = strings.Contains(m.Subject, "sim-x1") && r.ent%5 == 1 || !strings.Contains(m.Subject, "sim-x1") && r.ent%50 == 1
	}
	if log {
		var payload any = d
		if d == nil {
			s := string(m.Data)
			if len(s) > 1500 {
				s = s[:1500] + "..."
			}
			payload = s
		}
		r.write(map[string]any{"kind": "bus", "subject": m.Subject, "data": payload})
	}
}

func (r *recorder) write(v map[string]any) {
	v["at"] = time.Now().UTC().Format(time.RFC3339Nano)
	b, _ := json.Marshal(v)
	fmt.Fprintln(r.f, string(b))
}

func (r *recorder) note(kind string, v any) {
	r.mu.Lock()
	defer r.mu.Unlock()
	r.write(map[string]any{"kind": kind, "data": v})
}

func (r *recorder) all() []rec {
	r.mu.Lock()
	defer r.mu.Unlock()
	return append([]rec(nil), r.msgs...)
}

func (r *recorder) find(prefix string, pred func(rec) bool) []rec {
	var out []rec
	for _, m := range r.all() {
		if strings.HasPrefix(m.Subject, prefix) && (pred == nil || pred(m)) {
			out = append(out, m)
		}
	}
	return out
}

func dget(m map[string]any, path ...string) any {
	var cur any = m
	for _, p := range path {
		mm, ok := cur.(map[string]any)
		if !ok {
			return nil
		}
		cur = mm[p]
	}
	return cur
}

func (r *recorder) actionEvents(actionID string) []string {
	var ev []string
	for _, m := range r.find("evt.so.action.", func(m rec) bool { return dget(m.Data, "data", "action_id") == actionID }) {
		ev = append(ev, strings.TrimPrefix(m.Subject, "evt.so.action."))
	}
	return ev
}

func (r *recorder) actionCode(actionID, event string) string {
	for _, m := range r.find("evt.so.action."+event, func(m rec) bool { return dget(m.Data, "data", "action_id") == actionID }) {
		c, _ := dget(m.Data, "data", "code").(string)
		return c
	}
	return ""
}

// ---------- harness ----------

type H struct {
	url   string
	ws    string
	pw    map[string]string
	conns map[string]*nats.Conn
	procs []*exec.Cmd
	names []string
	r     *recorder
	res   []map[string]any
	bugs  []string
}

func (h *H) conn(user string) *nats.Conn {
	if c, ok := h.conns[user]; ok {
		return c
	}
	c, err := nats.Connect(h.url, nats.UserInfo(user, h.pw[user]), nats.Name("e2e-"+user),
		nats.ErrorHandler(func(_ *nats.Conn, s *nats.Subscription, err error) {
			h.r.note("nats-async-error", map[string]any{"user": user, "err": err.Error()})
		}))
	if err != nil {
		panic(fmt.Sprintf("connect %s: %v", user, err))
	}
	h.conns[user] = c
	return c
}

func (h *H) req(user, subj string, body any) (map[string]any, error) {
	b, _ := json.Marshal(body)
	m, err := h.conn(user).Request(subj, b, 6*time.Second)
	if err != nil {
		h.r.note("rpc", map[string]any{"user": user, "subject": subj, "request": body, "error": err.Error()})
		return nil, err
	}
	var v map[string]any
	_ = json.Unmarshal(m.Data, &v)
	h.r.note("rpc", map[string]any{"user": user, "subject": subj, "request": body, "reply": v, "reply_headers": m.Header})
	return v, nil
}

func (h *H) start(name string, env []string, bin string, args ...string) {
	logf, err := os.Create(filepath.Join(EV, "logs", name+".log"))
	if err != nil {
		panic(err)
	}
	c := exec.Command(bin, args...)
	c.Env = append(os.Environ(), env...)
	c.Stdout, c.Stderr = logf, logf
	if err := c.Start(); err != nil {
		panic(fmt.Sprintf("start %s: %v", name, err))
	}
	h.procs = append(h.procs, c)
	h.names = append(h.names, name)
	h.r.note("step", map[string]any{"started": name, "pid": c.Process.Pid, "args": args})
}

func (h *H) stopAll() {
	for _, c := range h.conns {
		c.Close()
	}
	for i := len(h.procs) - 1; i >= 0; i-- {
		c := h.procs[i]
		_ = c.Process.Signal(os.Interrupt)
		done := make(chan struct{})
		go func() { _ = c.Wait(); close(done) }()
		select {
		case <-done:
		case <-time.After(6 * time.Second):
			_ = c.Process.Kill()
			<-done
		}
		h.r.note("step", map[string]any{"stopped": h.names[i]})
	}
}

func waitFor(d time.Duration, cond func() bool) bool {
	deadline := time.Now().Add(d)
	for time.Now().Before(deadline) {
		if cond() {
			return true
		}
		time.Sleep(100 * time.Millisecond)
	}
	return false
}

func freePort(network string) int {
	if network == "udp" {
		c, _ := net.ListenPacket("udp", "127.0.0.1:0")
		defer c.Close()
		return c.LocalAddr().(*net.UDPAddr).Port
	}
	l, _ := net.Listen("tcp", "127.0.0.1:0")
	defer l.Close()
	return l.Addr().(*net.TCPAddr).Port
}

func uuid4() string {
	var u [16]byte
	_, _ = rand.Read(u[:])
	u[6] = u[6]&0x0f | 0x40
	u[8] = u[8]&0x3f | 0x80
	return fmt.Sprintf("%x-%x-%x-%x-%x", u[0:4], u[4:6], u[6:8], u[8:10], u[10:16])
}

func newID(prefix string) string {
	const cs = "0123456789abcdefghjkmnpqrstvwxyz"
	b := make([]byte, 26)
	_, _ = rand.Read(b)
	for i := range b {
		b[i] = cs[int(b[i])%32]
	}
	return prefix + "-" + string(b)
}

// destination: point at bearing (deg) and distance (m) from lat/lon.
func destination(lat, lon, brg, dist float64) (float64, float64) {
	R := 6371008.8
	φ1, λ1, θ, δ := lat*math.Pi/180, lon*math.Pi/180, brg*math.Pi/180, dist/R
	φ2 := math.Asin(math.Sin(φ1)*math.Cos(δ) + math.Cos(φ1)*math.Sin(δ)*math.Cos(θ))
	λ2 := λ1 + math.Atan2(math.Sin(θ)*math.Sin(δ)*math.Cos(φ1), math.Cos(δ)-math.Sin(φ1)*math.Sin(φ2))
	return φ2 * 180 / math.Pi, λ2 * 180 / math.Pi
}

func haversine(lat1, lon1, lat2, lon2 float64) float64 {
	R := 6371008.8
	dφ, dλ := (lat2-lat1)*math.Pi/180, (lon2-lon1)*math.Pi/180
	a := math.Sin(dφ/2)*math.Sin(dφ/2) + math.Cos(lat1*math.Pi/180)*math.Cos(lat2*math.Pi/180)*math.Sin(dλ/2)*math.Sin(dλ/2)
	return 2 * R * math.Asin(math.Sqrt(a))
}

func actionReq(actionType, entityType, id string, params map[string]any) map[string]any {
	return map[string]any{"v": 1, "request_id": uuid4(), "action_type": actionType, "action_version": 1,
		"target": map[string]any{"entity_type": entityType, "entity_id": id}, "params": params,
		"requested_by": map[string]any{"kind": "user", "id": "user:claimed-by-body"}, "reason": "e2e integration"}
}

func family(t string) string {
	if strings.HasPrefix(t, "task.") || strings.HasPrefix(t, "mission.") {
		return "task"
	}
	return "command"
}

// approve: propose as proposer, confirm with each approver. Returns action id, propose reply, error.
func (h *H) approve(proposer string, body map[string]any, approvers ...string) (string, map[string]any, error) {
	fam := family(body["action_type"].(string))
	p, err := h.req(proposer, "svc.policy."+fam+".propose", body)
	if err != nil {
		return "", nil, err
	}
	if p["status"] != "awaiting_confirmation" {
		return fmt.Sprint(p["action_id"]), p, fmt.Errorf("propose %s: %v", body["action_type"], p)
	}
	id := p["action_id"].(string)
	for _, a := range approvers {
		c, err := h.req(a, "svc.policy."+fam+".confirm", map[string]any{"v": 1, "action_id": id, "params_hash": p["params_hash"], "decision": "approve"})
		if err != nil {
			return id, p, err
		}
		if c["denial"] != nil {
			return id, p, fmt.Errorf("confirm %s by %s: %v", body["action_type"], a, c)
		}
	}
	return id, p, nil
}

func (h *H) result(step, result, detail string, extra map[string]any) {
	m := map[string]any{"step": step, "result": result, "detail": detail}
	for k, v := range extra {
		m[k] = v
	}
	h.res = append(h.res, m)
	h.r.note("result", m)
	fmt.Printf("[%s] %s: %s\n", strings.ToUpper(result), step, detail)
}

func (h *H) bug(s string) {
	h.bugs = append(h.bugs, s)
	h.r.note("bug", s)
	fmt.Println("BUG:", s)
}

// lastEnt reads the newest recorded ent record for a subject.
func (h *H) lastEnt(subj string) map[string]any {
	ms := h.r.find(subj, nil)
	if len(ms) == 0 {
		return nil
	}
	return ms[len(ms)-1].Data
}

func (h *H) dronePos() (float64, float64, bool) {
	e := h.lastEnt("ent.drone.sim-x1.state")
	if e == nil {
		return 0, 0, false
	}
	lat, ok1 := dget(e, "location", "lat").(float64)
	lon, ok2 := dget(e, "location", "lon").(float64)
	return lat, lon, ok1 && ok2
}

func envOr(k, d string) string {
	if v := os.Getenv(k); v != "" {
		return v
	}
	return d
}

func main() {
	must := func(err error) {
		if err != nil {
			panic(err)
		}
	}
	_ = os.MkdirAll(filepath.Join(EV, "logs"), 0o755)
	tf, err := os.Create(filepath.Join(EV, "transcript.jsonl"))
	must(err)
	defer tf.Close()
	h := &H{conns: map[string]*nats.Conn{}, r: &recorder{f: tf}}
	b, err := os.ReadFile(filepath.Join(RUN, "passwords.json"))
	must(err)
	must(json.Unmarshal(b, &h.pw))

	defer func() {
		if p := recover(); p != nil {
			h.result("harness", "fail", fmt.Sprint("harness aborted: ", p), nil)
		}
		h.writeResults()
		h.stopAll()
	}()

	// ---- nats-server ----
	port, wsPort := freePort("tcp"), freePort("tcp")
	out, err := exec.Command(PY, "/Users/danielcarku/Desktop/work/.run/tmp-e2e/harness/mkconf.py", RUN, fmt.Sprint(port), fmt.Sprint(wsPort), map[bool]string{true: "dev", false: "plain"}[DEV]).CombinedOutput()
	h.r.note("step", map[string]any{"mkconf": string(out)})
	must(err)
	b, _ = os.ReadFile(filepath.Join(RUN, "passwords.json"))
	must(json.Unmarshal(b, &h.pw))
	_ = os.RemoveAll(filepath.Join(RUN, "js"))
	h.start("nats-server", nil, NATS, "-c", filepath.Join(RUN, "nats.conf"))
	h.url = fmt.Sprintf("nats://127.0.0.1:%d", port)
	h.ws = fmt.Sprintf("ws://127.0.0.1:%d", wsPort)
	if !waitFor(10*time.Second, func() bool {
		c, err := nats.Connect(h.url, nats.UserInfo("onemind", h.pw["onemind"]))
		if err == nil {
			c.Close()
		}
		return err == nil
	}) {
		panic("nats-server did not come up")
	}
	admin := h.conn("onemind") // legacy shared user, stage B deny lists apply

	// ---- streams and buckets from the CRs ----
	var streams []struct {
		Kind   string         `json:"kind"`
		Config map[string]any `json:"config"`
	}
	b, _ = os.ReadFile(filepath.Join(RUN, "streams.json"))
	must(json.Unmarshal(b, &streams))
	for _, s := range streams {
		cb, _ := json.Marshal(s.Config)
		m, err := admin.Request("$JS.API.STREAM.CREATE."+s.Config["name"].(string), cb, 5*time.Second)
		must(err)
		var rep map[string]any
		_ = json.Unmarshal(m.Data, &rep)
		if rep["error"] != nil {
			panic(fmt.Sprintf("create %s: %v", s.Config["name"], rep["error"]))
		}
	}
	h.r.note("step", map[string]any{"streams_created": len(streams)})

	// ---- recorder subscriptions (onemind, read side) ----
	for _, s := range []string{"cmd.>", "evt.>", "tak.cot.>", "tel.fleet.>", "tel.feed.>", "intel.>", "ent.>"} {
		_, err := admin.Subscribe(s, h.r.add)
		must(err)
	}
	must(admin.Flush())

	// ---- keys, principals, tuples ----
	pub, priv, _ := ed25519.GenerateKey(rand.Reader)
	der, _ := x509.MarshalPKCS8PrivateKey(priv)
	keyPath := filepath.Join(RUN, "policy-signing.pem")
	must(os.WriteFile(keyPath, pem.EncodeToMemory(&pem.Block{Type: "PRIVATE KEY", Bytes: der}), 0o600))
	keysDir := filepath.Join(RUN, "policy-keys")
	_ = os.MkdirAll(keysDir, 0o755)
	pder, _ := x509.MarshalPKIXPublicKey(pub)
	must(os.WriteFile(filepath.Join(keysDir, "e2e.pem"), pem.EncodeToMemory(&pem.Block{Type: "PUBLIC KEY", Bytes: pder}), 0o644))

	principals := map[string]any{"own_account": "ONEMIND", "agent_accounts": []string{"AGENTS"}, "principals": []map[string]any{
		{"account": "CLIENT", "user": "zeus", "kind": "user", "id": "user:zeus"},
		{"account": "CLIENT", "user": "alice", "kind": "user", "id": "user:alice"},
		{"account": "AGENTS", "user": "hermes", "kind": "agent", "id": "agent:hermes/zeus/legacy"},
	}}
	b, _ = json.Marshal(principals)
	prPath := filepath.Join(RUN, "principals.json")
	must(os.WriteFile(prPath, b, 0o600))
	layerPr := map[string]any{"own_account": "ONEMIND", "org_group": "homestead", "principals": []map[string]any{
		{"account": "CLIENT", "user": "zeus", "id": "user:zeus", "groups": []string{"homestead"}},
		{"account": "CLIENT", "user": "alice", "id": "user:alice", "groups": []string{"homestead"}},
	}}
	b, _ = json.Marshal(layerPr)
	lprPath := filepath.Join(RUN, "layer-principals.json")
	must(os.WriteFile(lprPath, b, 0o600))
	var tuples []map[string]string
	for _, u := range []string{"user:zeus", "user:alice"} {
		tuples = append(tuples, map[string]string{"user": u, "relation": "operator", "object": "asset:sim-x1"})
	}
	acts := []string{"asset.arm", "asset.takeoff", "asset.goto", "asset.estop", "asset.rtl", "asset.land", "zone.create", "zone.update", "zone.remove",
		"mission.create", "mission.waypoint", "mission.complete", "mission.abort", "task.assign", "task.abort"}
	for _, a := range acts {
		for _, u := range []string{"user:zeus", "user:alice"} {
			for _, rel := range []string{"proposer", "approver", "second_approver"} {
				tuples = append(tuples, map[string]string{"user": u, "relation": rel, "object": "action_type:" + a})
			}
		}
		tuples = append(tuples, map[string]string{"user": "agent:hermes/zeus/legacy", "relation": "proposer", "object": "action_type:" + a})
	}
	b, _ = json.Marshal(tuples)
	tuPath := filepath.Join(RUN, "tuples.json")
	must(os.WriteFile(tuPath, b, 0o600))

	// ---- NWS stub (recorded fixture from feat/ingest-framework; no internet) ----
	fixture, err := os.ReadFile(filepath.Join(SRC, "ingest-framework/services/bridge/ingest/testdata/fixtures/nws/active-2026-10-29.json"))
	must(err)
	stubPort := freePort("tcp")
	go func() {
		_ = http.ListenAndServe(fmt.Sprintf("127.0.0.1:%d", stubPort), http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			w.Header().Set("Content-Type", "application/geo+json")
			_, _ = w.Write(fixture)
		}))
	}()
	srcYAML := fmt.Sprintf(`leaseTTL: 10s
statusInterval: 5s
defaults:
  headers:
    User-Agent: "(omos-ingest e2e, local stub)"
  rateLimit: {every: 1s, burst: 1}
sources:
  - id: nws-alerts
    registry: f094-noaa-nws-alerts
    kind: rest_poll
    parser: nws-alerts
    url: http://127.0.0.1:%d/alerts/active
    subject: intel.weather.noaa.alert
    schema: intel/weather-alert.v1.schema.json
    interval: 15s
    staleAfter: 10m
    headers: {Accept: application/geo+json}
    terms:
      licence: "open data, free to use for any purpose"
      url: https://www.weather.gov/documentation/services-web-api
`, stubPort)
	srcPath := filepath.Join(RUN, "sources.e2e.yaml")
	must(os.WriteFile(srcPath, []byte(srcYAML), 0o644))

	nenv := func(user string) []string {
		return []string{"NATS_URL=" + h.url, "NATS_USER=" + user, "NATS_PASSWORD=" + h.pw[user]}
	}
	hp := map[string]int{}
	for _, n := range []string{"entity", "layer", "projector", "geofence", "mission", "policy", "ingest"} {
		hp[n] = freePort("tcp")
	}
	schemas := filepath.Join(SRC, "policy-service/services/schemas")

	// ---- processes ----
	udp := freePort("udp")
	h.start("sim-drone", []string{"MAVLINK20=1"}, PY, "-I", SIM, "--out", fmt.Sprintf("udpout:127.0.0.1:%d", udp),
		"--lat", fmt.Sprint(HOME[0]), "--lon", fmt.Sprint(HOME[1]), "--radius", "40", "--alt", "30", "--speed", "15")
	h.start("bridge-mavlink", append(nenv("bridge-mavlink-home"), "ASSET_ID=sim-x1", fmt.Sprintf("MAVLINK_ENDPOINT=udpserver:127.0.0.1:%d", udp),
		"POLICY_PUBKEYS=e2e="+base64.StdEncoding.EncodeToString(pub), "SIG_REQUIRED=true",
		fmt.Sprintf(`GEOFENCE_JSON={"keep_in":[{"id":"sim-range","circle":{"lat":%f,"lon":%f,"radius_m":2000}}],"max_hae_m":300}`, HOME[0], HOME[1])),
		filepath.Join(BIN, "bridge-mavlink"))
	h.start("entity", append(nenv("entity"), fmt.Sprintf("HTTP_ADDR=127.0.0.1:%d", hp["entity"]), "START_LOOKBACK=0"), filepath.Join(BIN, "entity"))
	h.start("layer", append(nenv("layer"), fmt.Sprintf("HTTP_ADDR=127.0.0.1:%d", hp["layer"]), "PRINCIPALS_FILE="+lprPath), filepath.Join(BIN, "layer"))
	h.start("projector", append(nenv("projector-gated"), fmt.Sprintf("HTTP_ADDR=127.0.0.1:%d", hp["projector"]), "PROJECTOR_TARGET=server", "TAK_RELEASE_GROUPS="+map[bool]string{true: "homestead", false: ""}[DEV]), filepath.Join(BIN, "projector"))
	h.start("geofence", append(nenv("geofence"), fmt.Sprintf("HTTP_ADDR=127.0.0.1:%d", hp["geofence"]), "POLICY_KEYS_DIR="+keysDir, "TICK=1s"), filepath.Join(BIN, "geofence"))
	h.start("mission", append(nenv("mission"), fmt.Sprintf("HEALTH_ADDR=127.0.0.1:%d", hp["mission"]), "MISSION_PRINCIPALS_FILE="+prPath,
		"MISSION_SWEEP_EVERY=200ms", "MISSION_ARRIVAL=true", "MISSION_ARRIVAL_RADIUS_M=25"), filepath.Join(BIN, "mission"))
	h.start("policy", append(nenv("policy"), fmt.Sprintf("HEALTH_ADDR=127.0.0.1:%d", hp["policy"]), "POLICY_AUTHZ=local",
		"POLICY_MODEL_FILE="+filepath.Join(SRC, "policy-service/platform/security/openfga/onemind-actions.json"),
		"POLICY_TUPLES_FILE="+tuPath, "POLICY_SCHEMA_DIR="+schemas, "POLICY_PRINCIPALS_FILE="+prPath,
		"POLICY_SIGNING_KEY_FILE="+keyPath, "POLICY_SIGNING_KID=e2e"), filepath.Join(BIN, "policy")) // gitleaks:allow (key id "e2e" and a path variable, not a secret)
	h.start("ingest", nenv("ingest"), filepath.Join(BIN, "ingest"), "-config", srcPath,
		"-schemas", filepath.Join(SRC, "ingest-framework/services/schemas"), "-nats", h.url, "-instance", "e2e-1",
		"-listen", fmt.Sprintf("127.0.0.1:%d", hp["ingest"]))

	health := map[string]bool{}
	for n, p := range hp {
		path := "/healthz"
		u := fmt.Sprintf("http://127.0.0.1:%d%s", p, path)
		health[n] = waitFor(30*time.Second, func() bool {
			r, err := http.Get(u)
			if err == nil {
				r.Body.Close()
			}
			return err == nil && r.StatusCode == 200
		})
	}
	h.r.note("step", map[string]any{"health": health})
	fmt.Println("health:", health)
	for n, ok := range health {
		if !ok {
			h.bug(fmt.Sprintf("%s /healthz not 200 within 30 s on the shared stack (see logs/%s.log)", n, n))
		}
	}

	h.step1()
	h.setupFlight()
	h.step2()
	h.step3()
	h.step4()
	h.step5()
	h.step6()
	h.step7()
	h.invariant()

	// ---- hold the stack for the browser (step 8) ----
	st := map[string]any{"ws": h.ws, "nats": h.url, "users": map[string]any{
		"zeus":  map[string]string{"user": "zeus", "pass": h.pw["zeus"]},
		"alice": map[string]string{"user": "alice", "pass": h.pw["alice"]},
	}, "home": HOME}
	b, _ = json.Marshal(st)
	_ = os.Remove(filepath.Join(RUN, "stop"))
	must(os.WriteFile(filepath.Join(RUN, "stack.json"), b, 0o600))
	h.writeResults()
	fmt.Println("STACK READY", h.ws)
	if os.Getenv("E2E_NOHOLD") == "1" {
		return
	}
	waitFor(40*time.Minute, func() bool { _, err := os.Stat(filepath.Join(RUN, "stop")); return err == nil })
	h.afterBrowser()
}

func (h *H) writeResults() {
	b, _ := json.MarshalIndent(map[string]any{"steps": h.res, "bugs": h.bugs}, "", "  ")
	_ = os.WriteFile(filepath.Join(EV, "results.json"), b, 0o644)
}

// guard runs one step and turns a panic into a fail result.
func (h *H) guard(step string, fn func()) {
	defer func() {
		if p := recover(); p != nil {
			h.result(step, "fail", fmt.Sprint("step aborted: ", p), nil)
		}
	}()
	fn()
}

// ---------- step 1: telemetry -> fleet subjects -> entity with capabilities ----------

func (h *H) step1() {
	h.guard("1 telemetry to entity", func() {
		okTel := waitFor(30*time.Second, func() bool {
			return len(h.r.find("tel.fleet.drone.sim-x1.position", nil)) > 3 && len(h.r.find("tel.fleet.drone.sim-x1.status", nil)) > 0
		})
		var caps []any
		var ent map[string]any
		okEnt := waitFor(30*time.Second, func() bool {
			ent = h.lastEnt("ent.drone.sim-x1.state")
			caps, _ = dget(ent, "capabilities").([]any)
			return ent != nil && len(caps) > 0
		})
		subs := map[string]bool{}
		for _, m := range h.r.find("tel.fleet.", nil) {
			subs[m.Subject] = true
		}
		var sl []string
		for s := range subs {
			sl = append(sl, s)
		}
		sort.Strings(sl)
		var legacy = len(h.r.find("omos.", nil))
		hasGoto := false
		for _, c := range caps {
			hasGoto = hasGoto || c == "onemind.task.goto.v1"
		}
		detail := fmt.Sprintf("tel subjects %v; ent.drone.sim-x1.state capabilities=%v health=%v simulated=%v", sl, caps, dget(ent, "health"), dget(ent, "indicators", "simulated"))
		res := "pass"
		if !okTel || !okEnt || !hasGoto {
			res = "fail"
		}
		h.result("1 sim-x1 telemetry -> tel.fleet.drone.sim-x1.* -> ent.drone.sim-x1.state with capabilities", res, detail,
			map[string]any{"entity_sample": ent, "legacy_omos_subjects_seen": legacy})
	})
}

// ---------- flight setup: arm + takeoff (A4, two humans) and zones ----------

var zonePad, zoneRange = "zone-e2e-pad", "zone-e2e-range"

func circleZone(name, kind string, lat, lon, r float64) map[string]any {
	return map[string]any{"name": name, "geo": map[string]any{"shape": "circle", "kind": kind, "radius_m": r,
		"coords": map[string]any{"type": "Point", "coordinates": []float64{lon, lat}}}}
}

func (h *H) setupFlight() {
	h.guard("setup", func() {
		arm, _, err := h.approve("zeus", actionReq("asset.arm", "drone", "sim-x1", map[string]any{}), "zeus", "alice")
		if err != nil {
			panic(err)
		}
		okArm := waitFor(20*time.Second, func() bool { return contains(h.r.actionEvents(arm), "executed") })
		tko, _, err := h.approve("zeus", actionReq("asset.takeoff", "drone", "sim-x1", map[string]any{"hae_m": 60.0}), "zeus", "alice")
		if err != nil {
			panic(err)
		}
		okTko := waitFor(20*time.Second, func() bool { return contains(h.r.actionEvents(tko), "executed") })
		h.r.note("step", map[string]any{"arm": arm, "arm_events": h.r.actionEvents(arm), "takeoff": tko, "takeoff_events": h.r.actionEvents(tko)})
		if !okArm || !okTko {
			h.result("setup arm+takeoff (A4, zeus+alice)", "fail", fmt.Sprintf("arm %v takeoff %v", h.r.actionEvents(arm), h.r.actionEvents(tko)), nil)
			return
		}
		h.result("setup arm+takeoff (A4, zeus+alice)", "pass", fmt.Sprintf("arm events %v; takeoff events %v", h.r.actionEvents(arm), h.r.actionEvents(tko)), nil)
	})
}

func contains(l []string, s string) bool {
	for _, x := range l {
		if x == s {
			return true
		}
	}
	return false
}

// ---------- step 2: layer snapshot + delta includes sim-x1 (as CLIENT zeus) ----------

func (h *H) snapshot(user, layer string) ([]map[string]any, error) {
	c := h.conn(user)
	inbox := c.NewRespInbox()
	sub, err := c.SubscribeSync(inbox)
	if err != nil {
		return nil, err
	}
	defer sub.Unsubscribe()
	b, _ := json.Marshal(map[string]any{"v": 1, "layer": layer, "aoi": nil, "zoom": 12, "fields": "render", "max": 5000})
	if err := c.PublishRequest("svc.layer.snapshot", inbox, b); err != nil {
		return nil, err
	}
	var parts []map[string]any
	for {
		m, err := sub.NextMsg(6 * time.Second)
		if err != nil {
			return parts, err
		}
		var v map[string]any
		_ = json.Unmarshal(m.Data, &v)
		parts = append(parts, v)
		if v["last"] == true || v["error"] != nil {
			return parts, nil
		}
	}
}

func (h *H) step2() {
	h.guard("2 layer", func() {
		var parts []map[string]any
		var err error
		var found map[string]any
		ok := waitFor(30*time.Second, func() bool {
			parts, err = h.snapshot("zeus", "fleet")
			for _, p := range parts {
				items, _ := p["items"].([]any)
				for _, it := range items {
					if m, _ := it.(map[string]any); m != nil && m["id"] == "sim-x1" || m != nil && m["entity_id"] == "sim-x1" {
						found = m
					}
				}
			}
			return found != nil
		})
		h.r.note("check", map[string]any{"snapshot_parts": parts, "err": fmt.Sprint(err)})
		if !ok {
			h.result("2 layer snapshot+delta includes sim-x1", "fail", fmt.Sprintf("snapshot as CLIENT zeus did not contain sim-x1: parts=%v err=%v", trunc(parts), err), nil)
			return
		}
		seq, _ := parts[0]["ent_seq"].(float64)
		// watch from ent_seq+1
		c := h.conn("zeus")
		inbox := c.NewRespInbox()
		sub, _ := c.SubscribeSync(inbox)
		defer sub.Unsubscribe()
		wb, _ := json.Marshal(map[string]any{"v": 1, "layer": "fleet", "aoi": nil, "zoom": 12, "fields": "render", "from_seq": int64(seq) + 1, "lease_s": 60, "max_rate_hz": 4, "watch_id": nil})
		_ = c.PublishRequest("svc.layer.watch", inbox, wb)
		var first map[string]any
		var upserts []map[string]any
		var batches int
		deadline := time.Now().Add(20 * time.Second)
		for time.Now().Before(deadline) && len(upserts) < 2 {
			m, err := sub.NextMsg(3 * time.Second)
			if err != nil {
				continue
			}
			var v map[string]any
			_ = json.Unmarshal(m.Data, &v)
			if first == nil {
				first = v
				continue
			}
			batches++
			ops, _ := v["ops"].([]any)
			for _, o := range ops {
				om, _ := o.(map[string]any)
				e, _ := om["e"].(map[string]any)
				if e != nil && (e["id"] == "sim-x1" || e["entity_id"] == "sim-x1") {
					upserts = append(upserts, om)
				}
			}
		}
		if first != nil && first["watch_id"] != nil {
			ub, _ := json.Marshal(map[string]any{"v": 1, "watch_id": first["watch_id"]})
			_, _ = c.Request("svc.layer.unwatch", ub, 3*time.Second)
		}
		h.r.note("check", map[string]any{"watch_first": first, "sim_x1_ops": upserts, "batches": batches})
		hz, _ := h.snapshot("zeus", "hazards")
		nh := 0
		for _, p := range hz {
			items, _ := p["items"].([]any)
			nh += len(items)
		}
		res := "pass"
		if len(upserts) == 0 {
			res = "fail"
		}
		h.result("2 layer snapshot+delta includes sim-x1", res, fmt.Sprintf("snapshot item %v; watch ack %v; %d batches, %d sim-x1 ops (first: %v); hazards layer items (NWS): %d",
			trunc(found), first, batches, len(upserts), trunc(firstOr(upserts)), nh), nil)
	})
}

func firstOr(l []map[string]any) any {
	if len(l) == 0 {
		return nil
	}
	return l[0]
}

func trunc(v any) string {
	b, _ := json.Marshal(v)
	s := string(b)
	if len(s) > 600 {
		s = s[:600] + "..."
	}
	return s
}

// ---------- step 3: projector CoT; nothing from feeds on tak.cot.inject ----------

func (h *H) step3() {
	h.guard("3 projector", func() {
		if DEV {
			// Probe: a copy of the latest sim-x1 record as sim-x9 with releasable_to [homestead]
			// (entity.md section 4 example), published as the entity user, to show the projector
			// path works once the marking is right. Not the real entity service's output.
			e := h.lastEnt("ent.drone.sim-x1.state")
			if e != nil {
				cp := map[string]any{}
				b, _ := json.Marshal(e)
				_ = json.Unmarshal(b, &cp)
				cp["entity_id"] = "sim-x9"
				cp["seq"] = 1
				cp["marking"] = map[string]any{"tlp": "AMBER+STRICT", "releasable_to": []string{"homestead"}}
				now := time.Now().UnixMilli()
				for _, k := range []string{"t", "source_t", "observed_t", "updated_t"} {
					if _, ok := cp[k]; ok {
						cp[k] = now
					}
				}
				if _, ok := cp["expiry_t"]; ok {
					cp["expiry_t"] = now + 120000
				}
				pb, _ := json.Marshal(cp)
				_ = h.conn("entity").Publish("ent.drone.sim-x9.state", pb)
				_ = h.conn("entity").Flush()
				h.r.note("probe", map[string]any{"published": "ent.drone.sim-x9.state", "record": cp})
			}
			okp := waitFor(20*time.Second, func() bool {
				return len(h.r.find("tak.cot.inject", func(m rec) bool { return strings.Contains(string(m.Raw), "sim-x9") })) > 0
			})
			var raw string
			for _, m := range h.r.find("tak.cot.inject", func(m rec) bool { return strings.Contains(string(m.Raw), "sim-x9") }) {
				raw = string(m.Raw)
				break
			}
			h.r.note("check", map[string]any{"probe_projected": okp, "probe_cot": raw})
			h.result("3b probe: correctly marked sim-x9 record is projected (TAK_RELEASE_GROUPS=homestead)", map[bool]string{true: "pass", false: "fail"}[okp], fmt.Sprintf("projected=%v cot=%.600s", okp, raw), nil)
		}
		ok := waitFor(30*time.Second, func() bool {
			return len(h.r.find("tak.cot.inject", func(m rec) bool { return strings.Contains(string(m.Raw), "sim-x1") })) > 0
		})
		intel := h.r.find("intel.", nil)
		var feedEnts []string
		for _, m := range h.r.find("ent.", func(m rec) bool { return !strings.Contains(m.Subject, "sim-x1") }) {
			feedEnts = append(feedEnts, m.Subject)
		}
		inj := h.r.find("tak.cot.inject", nil)
		var uids []string
		var feedHits []string
		for _, m := range inj {
			s := string(m.Raw)
			i := strings.Index(s, `uid="`)
			uid := ""
			if i >= 0 {
				uid = s[i+5:]
				uid = uid[:strings.Index(uid, `"`)]
			}
			uids = append(uids, uid)
			low := strings.ToLower(s)
			if !strings.HasPrefix(uid, "OM.") || strings.Contains(low, "nws") || strings.Contains(low, "noaa") || strings.Contains(uid, "INTEL.") {
				feedHits = append(feedHits, uid)
			}
		}
		uniq := map[string]int{}
		for _, u := range uids {
			uniq[u]++
		}
		var sample string
		for _, m := range inj {
			if strings.Contains(string(m.Raw), "sim-x1") {
				sample = string(m.Raw)
				break
			}
		}
		h.r.note("check", map[string]any{"cot_sample": sample, "inject_uids": uniq, "intel_msgs": len(intel), "non_drone_ent_msgs": len(feedEnts)})
		res := "pass"
		if !ok || len(feedHits) > 0 || len(intel) == 0 {
			res = "fail"
		}
		h.result("3 projector emits sim-x1 CoT on tak.cot.inject; no feed data on tak.cot.inject", res,
			fmt.Sprintf("tak.cot.inject uids %v; feed-derived on inject: %v; intel.* msgs from ingest: %d; non-drone ent msgs (NWS-derived): %d; sim-x1 CoT: %.300s", uniq, feedHits, len(intel), len(feedEnts), sample), nil)
	})
}

// ---------- step 4: geofence drawn, drone exits, alert ----------

func (h *H) step4() {
	h.guard("4 geofence", func() {
		// Range keep-in (policy needs the goto point inside a keep-in) and a small pad keep-in.
		for _, z := range []struct {
			id string
			r  float64
		}{{zoneRange, 1800}, {zonePad, 250}} {
			id, _, err := h.approve("zeus", actionReq("zone.create", "zone", z.id, circleZone(z.id, "keep_in", HOME[0], HOME[1], z.r)), "zeus")
			if err != nil {
				h.result("4 geofence drawn, drone exits, alert raised", "fail", "zone.create: "+err.Error(), nil)
				return
			}
			if !waitFor(15*time.Second, func() bool { return contains(h.r.actionEvents(id), "executed") }) {
				h.result("4 geofence drawn, drone exits, alert raised", "fail", fmt.Sprintf("zone.create %s events %v", z.id, h.r.actionEvents(id)), nil)
				return
			}
		}
		time.Sleep(2 * time.Second)
		lat, lon := destination(HOME[0], HOME[1], 90, 700)
		gid, _, err := h.approve("zeus", actionReq("asset.goto", "drone", "sim-x1", map[string]any{"lat": lat, "lon": lon, "hae_m": 60.0, "speed_mps": 15.0}), "zeus")
		if err != nil {
			h.result("4 geofence drawn, drone exits, alert raised", "fail", "goto out of the pad: "+err.Error(), nil)
			return
		}
		okExit := waitFor(60*time.Second, func() bool {
			return len(h.r.find("evt.map.geofence."+zonePad+".exit", nil)) > 0
		})
		var alert map[string]any
		okAlert := waitFor(15*time.Second, func() bool {
			for _, m := range h.r.find("evt.so.alert.raised", nil) {
				if strings.Contains(string(m.Raw), zonePad) && strings.Contains(string(m.Raw), "sim-x1") {
					alert = m.Data
					return true
				}
			}
			return false
		})
		al, _ := h.req("zeus", "svc.geofence.alerts", map[string]any{"entity_id": "sim-x1"})
		res := "pass"
		if !okExit || !okAlert {
			res = "fail"
		}
		h.result("4 geofence drawn, drone exits, alert raised", res, fmt.Sprintf("zones %s(1800 m) and %s(250 m) via zone.create+zeus confirm; goto %s events %v; evt.map.geofence.%s.exit=%v; alert=%s; svc.geofence.alerts (CLIENT zeus)=%s",
			zoneRange, zonePad, gid, h.r.actionEvents(gid), zonePad, okExit, trunc(alert), trunc(al)), nil)
	})
}

// ---------- step 5: mission, task goto via policy, agent confirm refused, human accepted ----------

func (h *H) step5() {
	h.guard("5 mission", func() {
		msn := newID("msn")
		mc, _, err := h.approve("zeus", actionReq("mission.create", "mission", msn, map[string]any{"name": "E2E north check", "intent": "integration"}), "zeus")
		if err != nil {
			h.result("5 mission+task goto through policy", "fail", "mission.create: "+err.Error(), nil)
			return
		}
		waitFor(15*time.Second, func() bool { return contains(h.r.actionEvents(mc), "executed") })
		tsk := newID("tsk")
		glat, glon := destination(HOME[0], HOME[1], 0, 600)
		body := actionReq("task.assign", "drone", "sim-x1", map[string]any{"task_id": tsk, "parent_id": msn,
			"type": "onemind.task.goto.v1", "spec": map[string]any{"lat": glat, "lon": glon, "hae_m": 60.0}, "description": "fly north 600 m"})
		body["requested_by"] = map[string]any{"kind": "agent", "id": "agent:hermes/zeus/legacy", "on_behalf_of": "user:zeus"}
		p, err := h.req("hermes", "svc.policy.task.propose", body)
		if err != nil || p["status"] != "awaiting_confirmation" {
			h.result("5 mission+task goto through policy", "fail", fmt.Sprintf("hermes propose: %v %v", p, err), nil)
			return
		}
		act := p["action_id"].(string)
		time.Sleep(1500 * time.Millisecond)
		early := len(h.r.find("cmd.fleet.drone.sim-x1.goto", func(m rec) bool { return dget(m.Data, "action_id") == act }))
		// Agent confirm: through its own account (server must refuse), both endpoint families.
		_, errA := h.conn("hermes").Request("svc.policy.task.confirm", []byte(fmt.Sprintf(`{"v":1,"action_id":%q,"params_hash":%q,"decision":"approve"}`, act, p["params_hash"])), 2*time.Second)
		_, errB := h.conn("hermes").Request("svc.policy.command.confirm", []byte(fmt.Sprintf(`{"v":1,"action_id":%q,"params_hash":%q,"decision":"approve"}`, act, p["params_hash"])), 2*time.Second)
		agentRefused := errA != nil && errB != nil
		get, _ := h.req("zeus", "svc.policy.action.get", map[string]any{"v": 1, "action_id": act})
		c, err := h.req("zeus", "svc.policy.task.confirm", map[string]any{"v": 1, "action_id": act, "params_hash": p["params_hash"], "decision": "approve"})
		if err != nil {
			h.result("5 mission+task goto through policy", "fail", "zeus confirm: "+err.Error(), nil)
			return
		}
		var final map[string]any
		okDone := waitFor(90*time.Second, func() bool {
			e, err := h.kvGet("mission-state", tsk)
			if err != nil {
				return false
			}
			final = e
			st, _ := final["status"].(map[string]any)
			s, _ := st["state"].(string)
			return s == "done_ok" || s == "done_not_ok"
		})
		dlat, dlon, _ := h.dronePos()
		dist := haversine(dlat, dlon, glat, glon)
		var cmd map[string]any
		for _, m := range h.r.find("cmd.fleet.drone.sim-x1.goto", func(m rec) bool { return dget(m.Data, "action_id") == act }) {
			cmd = m.Data
		}
		var outcomes []string
		for _, m := range h.r.find("evt.drone.sim-x1.command", func(m rec) bool { return dget(m.Data, "data", "action_id") == act }) {
			o, _ := dget(m.Data, "data", "outcome").(string)
			cd, _ := dget(m.Data, "data", "code").(string)
			outcomes = append(outcomes, o+cdSuffix(cd))
		}
		var taskReports []string
		for _, m := range h.r.find("evt.drone.sim-x1.task", nil) {
			taskReports = append(taskReports, fmt.Sprint(dget(m.Data, "data", "state")))
		}
		facts := func(id string) []string {
			var f []string
			for _, m := range h.r.find("evt.mission."+id+".", nil) {
				f = append(f, strings.TrimPrefix(m.Subject, "evt.mission."+id+"."))
			}
			return f
		}
		st, _ := final["status"].(map[string]any)
		ev := h.r.actionEvents(act)
		res := "pass"
		why := []string{}
		if early != 0 {
			why = append(why, "goto sent before human confirm")
		}
		if !agentRefused {
			why = append(why, "agent confirm not refused")
		}
		if c["status"] != "dispatched" {
			why = append(why, fmt.Sprintf("confirm status %v", c["status"]))
		}
		if !okDone || st["state"] != "done_ok" {
			why = append(why, fmt.Sprintf("task state %v", st["state"]))
		}
		for _, e := range []string{"proposed", "approved", "dispatched"} {
			if !contains(ev, e) {
				why = append(why, "no evt.so.action."+e)
			}
		}
		if cmd == nil || dget(cmd, "approval", "params_hash") == nil || dget(cmd, "sig") == nil {
			why = append(why, "fleet command missing/unsigned/no params_hash")
		}
		if !contains(outcomes, "acknowledged") {
			why = append(why, "bridge did not acknowledge (params_hash/sig verification)")
		}
		if dist > 60 {
			why = append(why, fmt.Sprintf("drone %.0f m from goal", dist))
		}
		if len(why) > 0 {
			res = "fail"
		}
		h.result("5 mission + task goto via policy; agent confirm refused; human confirm accepted; bridge verifies; drone flies; mission-state; audit", res,
			fmt.Sprintf("mission %s create events %v; task %s action %s: hermes propose=awaiting_confirmation, fleet cmds before confirm=%d, hermes confirm (task: %v; command: %v), zeus confirm=%v; action events %v; cmd approval=%s; bridge outcomes %v; bridge task reports %v; KV mission-state final=%s; task facts %v; mission facts %v; drone %.0f m from goal. problems: %v",
				msn, h.r.actionEvents(mc), tsk, act, early, errA, errB, c["status"], ev, trunc(dget(cmd, "approval")), outcomes, taskReports, trunc(st), facts(tsk), facts(msn), dist, why),
			map[string]any{"action_get_before_confirm": get})
	})
}

func cdSuffix(c string) string {
	if c == "" {
		return ""
	}
	return "(" + c + ")"
}

func (h *H) kvGet(bucket, key string) (map[string]any, error) {
	m, err := h.conn("onemind").Request("$JS.API.DIRECT.GET.KV_"+bucket+".$KV."+bucket+"."+key, nil, 3*time.Second)
	if err != nil {
		return nil, err
	}
	if st := m.Header.Get("Status"); st != "" {
		return nil, fmt.Errorf("status %s", st)
	}
	var v map[string]any
	if err := json.Unmarshal(m.Data, &v); err != nil {
		return nil, err
	}
	return v, nil
}

// ---------- step 6: estop while a task is pending ----------

func (h *H) step6() {
	h.guard("6 estop", func() {
		tsk := newID("tsk")
		glat, glon := destination(HOME[0], HOME[1], 200, 500)
		body := actionReq("task.assign", "drone", "sim-x1", map[string]any{"task_id": tsk, "parent_id": nil,
			"type": "onemind.task.goto.v1", "spec": map[string]any{"lat": glat, "lon": glon, "hae_m": 60.0}})
		body["requested_by"] = map[string]any{"kind": "agent", "id": "agent:hermes/zeus/legacy", "on_behalf_of": "user:zeus"}
		p, err := h.req("hermes", "svc.policy.task.propose", body)
		if err != nil || p["status"] != "awaiting_confirmation" {
			h.result("6 estop while a task is pending", "fail", fmt.Sprintf("pending task propose: %v %v", p, err), nil)
			return
		}
		pend := p["action_id"].(string)
		t0 := time.Now()
		eid, ep, err := h.approve("zeus", actionReq("asset.estop", "drone", "sim-x1", map[string]any{}), "zeus")
		lat := time.Since(t0)
		if err != nil {
			h.result("6 estop while a task is pending", "fail", fmt.Sprintf("estop: %v (propose reply %v)", err, ep), nil)
			return
		}
		waitFor(15*time.Second, func() bool {
			ev := h.r.actionEvents(eid)
			return contains(ev, "executed") || contains(ev, "failed") || contains(ev, "refused")
		})
		var cmdAt time.Time
		for _, m := range h.r.find("cmd.fleet.drone.sim-x1.estop", func(m rec) bool { return dget(m.Data, "action_id") == eid }) {
			cmdAt = m.At
		}
		var outcomes []string
		var mav any
		for _, m := range h.r.find("evt.drone.sim-x1.command", func(m rec) bool { return dget(m.Data, "data", "action_id") == eid }) {
			o, _ := dget(m.Data, "data", "outcome").(string)
			cd, _ := dget(m.Data, "data", "code").(string)
			outcomes = append(outcomes, o+cdSuffix(cd))
			if v := dget(m.Data, "data", "mavlink"); v != nil {
				mav = v
			}
		}
		pg, _ := h.req("zeus", "svc.policy.action.get", map[string]any{"v": 1, "action_id": pend})
		// reject the still-pending task so it does not linger
		_, _ = h.req("zeus", "svc.policy.task.confirm", map[string]any{"v": 1, "action_id": pend, "params_hash": p["params_hash"], "decision": "reject", "reason": "e2e cleanup after estop"})
		ev := h.r.actionEvents(eid)
		res := "pass"
		detail := fmt.Sprintf("pending task action %s (status %v while estop ran); estop %s propose->confirm %.0f ms, events %v, cmd.fleet.drone.sim-x1.estop published=%v, bridge outcomes %v, mavlink %v",
			pend, dget(pg, "action", "status"), eid, float64(lat.Milliseconds()), ev, !cmdAt.IsZero(), outcomes, mav)
		if cmdAt.IsZero() || !contains(outcomes, "acknowledged") {
			res = "fail"
		} else if !contains(ev, "executed") {
			res = "fail"
			detail += "; estop reached the vehicle but did not execute (see bugs: the scripted sim answers MAV_CMD_DO_FLIGHTTERMINATION with UNSUPPORTED)"
		}
		h.result("6 estop works while a task is pending", res, detail, nil)
	})
}

// ---------- step 7: stop-line verb rejected by name ----------

func (h *H) step7() {
	h.guard("7 stop line", func() {
		type tc struct {
			who, subj string
			body      map[string]any
		}
		strike := actionReq("asset.strike", "drone", "sim-x1", map[string]any{"lat": HOME[0], "lon": HOME[1]})
		intercept := actionReq("asset.intercept", "drone", "sim-x1", map[string]any{})
		engageTask := actionReq("task.assign", "drone", "sim-x1", map[string]any{"task_id": newID("tsk"), "parent_id": nil, "type": "onemind.task.engage.v1", "spec": map[string]any{}})
		engageTask["requested_by"] = map[string]any{"kind": "agent", "id": "agent:hermes/zeus/legacy", "on_behalf_of": "user:zeus"}
		cases := []tc{{"zeus", "svc.policy.command.propose", strike}, {"hermes", "svc.policy.command.propose", intercept}, {"hermes", "svc.policy.task.propose", engageTask}}
		var outs []string
		allOK := true
		fleetBefore := len(h.r.find("cmd.fleet.", nil))
		for _, c := range cases {
			r, err := h.req(c.who, c.subj, c.body)
			d, _ := r["denial"].(map[string]any)
			code, _ := d["code"].(string)
			msg, _ := d["message"].(string)
			verb := strings.TrimPrefix(fmt.Sprint(c.body["action_type"]), "asset.")
			if c.body["action_type"] == "task.assign" {
				verb = "engage"
			}
			named := strings.Contains(strings.ToLower(msg), verb)
			audited := false
			if aid, _ := r["action_id"].(string); aid != "" {
				audited = h.r.actionCode(aid, "denied") == code
			}
			outs = append(outs, fmt.Sprintf("%s %s %v -> status=%v code=%s message=%q named=%v audited=%v err=%v", c.who, c.subj, c.body["action_type"], r["status"], code, msg, named, audited, err))
			if code != "forbidden_action" || !named {
				allOK = false
			}
		}
		time.Sleep(500 * time.Millisecond)
		if len(h.r.find("cmd.fleet.", nil)) != fleetBefore {
			allOK = false
			outs = append(outs, "a fleet command was published after a stop-line proposal")
		}
		res := "pass"
		if !allOK {
			res = "fail"
		}
		h.result("7 stop-line verb rejected by name", res, strings.Join(outs, " | "), nil)
	})
}

func (h *H) invariant() {
	h.guard("invariant", func() {
		approved := map[string]bool{}
		for _, m := range h.r.find("evt.so.action.approved", nil) {
			if a, _ := dget(m.Data, "data", "action_id").(string); a != "" {
				approved[a] = true
			}
		}
		bad := 0
		n := 0
		for _, m := range h.r.find("cmd.fleet.", nil) {
			n++
			if a, _ := dget(m.Data, "action_id").(string); !approved[a] {
				bad++
			}
		}
		res := "pass"
		if bad > 0 {
			res = "fail"
		}
		h.result("invariant: every cmd.fleet.> has a human evt.so.action.approved", res, fmt.Sprintf("%d fleet commands, %d without approval", n, bad), nil)
	})
}

// afterBrowser records what the bus saw while the browser step ran.
func (h *H) afterBrowser() {
	var gotos []map[string]any
	for _, m := range h.r.find("evt.so.action.", func(m rec) bool { return dget(m.Data, "data", "action_type") == "asset.goto" }) {
		gotos = append(gotos, map[string]any{"subject": m.Subject, "action_id": dget(m.Data, "data", "action_id"), "requested_by": dget(m.Data, "data", "requested_by"), "at": m.At.Format(time.RFC3339Nano)})
	}
	h.r.note("browser-window-audit", gotos)
	_ = hex.EncodeToString
}
