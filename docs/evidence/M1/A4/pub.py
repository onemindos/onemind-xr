import json, subprocess, sys
url, path = sys.argv[1], sys.argv[2]
n = 0
for line in open(path):
    fx = json.loads(line)
    subprocess.run(["nats", "-s", url, "pub", "--jetstream", fx["subject"], fx["data"]], check=True, capture_output=True)
    n += 1
print(f"published {n} fixture messages with JetStream ack")
