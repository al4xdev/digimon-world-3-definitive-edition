# Telemetry Harness + Plugin Gatekeeper — PRD v0.1

Status: draft for owner review · Owner: Alex · Evidence base: `deepseek-harness` @ `39163dd3` (cloned read-only at `/home/alex/git/my/deepseek-harness`)

## 1. Problem

We run agents that execute real actions on real machines. Three things are missing, and each is independently sufficient to lose control:

1. **No per-caller authorization.** Plugins execute with whatever ambient OS rights their process happens to hold. Nothing answers "may *this* caller, invoking *this* plugin, reach *this* resource?" — so a plugin shared by several callers is a confused deputy by construction.
2. **No unified telemetry.** Each team's agent logs in its own shape. There is no common event envelope, no tamper-evident ordering, and nothing lands in a durable store that survives the container.
3. **No control tower.** Nobody can see all teams in one place, approve access, set limits, or revoke a running capability.

## 2. Positioning against DeepSeek Harness (what DSH already has, and what it does not)

Verified in the clone, not assumed:

- DSH is genuinely "everything is a plugin" on Cordis (`README.md:7`); plugins are npm packages mounted by name + config in a profile YAML.
- DSH **has** a permission model, but it is **per session, tool-level, human-in-the-loop**: `--permission-mode <preset>` and `--approval-policy ask|never` (`docs/cli-help.md:119-122`), an `approval/request` waterfall event, and a `ctx.sandbox` process-sandbox seam (`docs/capability-seams.md:250-251`).
- DSH **explicitly disclaims** isolation strength: "Sandboxing, approval prompts, and permission controls can reduce risk, but they do not guarantee isolation" (`SAFETY.md`).
- DSH's `packages/guard` is loop hygiene (`repeat-tool-reminder`, `timeout-policy`), **not** an authorization gatekeeper. There is no per-client policy engine and no unified cross-team event journal.
- DSH already uses newline-delimited JSON-RPC over stdio for its Python SDK subprocess (`python/sdk/README.md`) — a wire convention we mirror rather than invent.

**Gap we fill:** per-caller authorization on shared plugin instances, static plugin admission control, kernel-enforced scoped space, hash-chained unified telemetry that survives the container, and a control tower that can revoke mid-run. DSH stays the reference for plugin *semantics*; we implement our own runtime in Python.

## 3. Users

| Persona | Needs from this |
|---|---|
| **Plugin developer** | Declare a plugin + its grants in a manifest; get a validated, watched, sandboxed process; get a clear denial reason instead of a stack trace. |
| **Team lead / operator** | One place to see what every team's agents did; grant access per caller; set limits; revoke without restarting everything. |
| **Control-tower admin** | Register services, hold policy, receive heartbeats, read the audit trail, kill a runaway. |
| **External consumer** (service, operator, CLI) | Call a plugin under its own identity, with rules that apply only to it. |

## 4. Goals

- **G1** Default-deny authorization where the effective verdict is computed per `(client, plugin, grant, resource)` on every mediated call.
- **G2** A plugin cannot obtain authority it did not declare, and cannot reach outside its scoped space — enforced by the kernel, tested against a hostile fixture that bypasses our own API.
- **G3** One append-only JSONL journal with a unified envelope, hash-chained, broker-assigned sequence, written outside any plugin-reachable path.
- **G4** Sealed segments land in the bucket as immutable new objects, never overwritten, and a crashed run reads as *incomplete* rather than complete.
- **G5** A control tower that registers, grants, limits, and revokes at runtime — with revocation reaching the data plane, not just a log line.
- **G6** A gatekeeper test suite where every assertion is judged by an **external oracle** (bytes on disk, processes alive, objects in the bucket), never by our own log.

## 5. Non-goals (v0.1)

- Production hardening, multi-host federation, or HA of the control tower.
- Rewriting arbitrary third-party plugins wholesale; the demo ships plugins we wrote.
- Cryptographic authenticity of the bucket (see §10, we prove integrity of transfer, not authorship).
- A tower UI. Registry + policy API and a CLI only; the dashboard is a later milestone.
- Model-quality work. The model driver is an interface with a stub for tests and an optional real call for the demo.

## 6. Terminology (and one naming decision)

DSH overloads **capability** to mean "swappable service seam" (`capability-seams.md`). Using it for permissions would make our docs unreadable against theirs. Therefore:

- **Grant** — a permission declaration: `read`, `write`, `spawn`, `handoff`, `ctx`, always resource-scoped. "Input/output" (model messages) is **not** a grant; it is the floor every plugin gets.
- **Client** — an authenticated external consumer (service, operator, CLI). **Open question Q1**: whether agents also appear as clients.
- **Plugin instance** — one supervised subprocess serving N clients.
- **Effective grant** — `manifest_grants ∩ client_allow − client_deny`, default deny, canonicalized resource prefixes.
- **Seal / segment** — rotation boundary; a segment is immutable once sealed.
- **Broker** — the only path from a client to a plugin's mediated effects.

## 7. Requirements

### A. Admission control (static, load time)
- **FR-A1** Plugin manifest (YAML) declares: name, version, entrypoint, artifact sha256, grants with resource prefixes, declared capabilities of the model face.
- **FR-A2** Validation rejects: unknown grant names, resources escaping the plugin root (after `realpath`), artifact hash mismatch, missing entrypoint, manifest schema violations.
- **FR-A3** Validation failure means **no process is spawned** and the failure is recorded with a reason code.
- **FR-A4** Declarative tools (YAML, no code) compile into a manifest + a fixed call plan; they get the same validation as code plugins.

### B. Supervision (runtime)
- **FR-B1** The watcher spawns each plugin as a child in its own process group, under `bwrap`: read-only `/`, private `/tmp`, network unshared by default, only the plugin's scoped dir bind-mounted read-write.
- **FR-B2** Watcher enforces start/stop/health/quarantine. Health is a mediated RPC, and health is **not** evidence of correct behaviour.
- **FR-B3** Global quarantine kills the process group (descendants included) and denies further calls.
- **FR-B4** The framework runtime never spawns plugins directly; plugins are children of the watcher, so the framework has no bypass.

### C. Authorization (per call)
- **FR-C1** Clients authenticate to the broker; `client_id` is derived from the authenticated transport (per-client socket + HMAC token), never from the message body.
- **FR-C2** Every mediated call is checked against the effective grant set before execution; denial returns a reason code and is journaled.
- **FR-C3** Per-client rules apply to a shared instance without killing it: revoking client B denies B's next call while A continues.
- **FR-C4** Plugins perform **no** authorization themselves and receive only a request-scoped handle; no authority is cached in the plugin across calls or across clients.
- **FR-C5** Resource scopes are per-client: B granted `write:regions/b` cannot reach `regions/a` even though both live in the same plugin instance.
- **FR-C6** Policy carries a monotonic `policy_version`; the broker checks it before each mediated call so revocation is not best-effort.

### D. Telemetry
- **FR-D1** Envelope: `team, agent, run_id, seq, ts, trace_id, event, actor, grant, resource, verdict, reason`.
- **FR-D2** `seq` is broker-assigned and monotonic; a plugin-supplied `seq` is rejected and journaled as a violation.
- **FR-D3** Each record carries `prev_hash`; the journal is hash-chained and any break is detectable by a reader.
- **FR-D4** The journal lives outside every plugin-reachable path; plugins cannot append, truncate, or replace it.
- **FR-D5** Segments seal on size, time, or `run_end`; a run ends with an explicit `run_complete` record.
- **FR-D6** Reads distinguish complete from incomplete runs.

### E. Sync (write-once bucket)
- **FR-E1** The sidecar uploads only sealed segments, keyed `{team}/{plugin|agent}/{date}/{run_id}/{seq_start}-{sha256}.jsonl`.
- **FR-E2** Uploads use `If-None-Match: *`; a pre-existing key is never overwritten — the sidecar treats `412` as success-by-identity and verifies the stored bytes' hash.
- **FR-E3** Retries are idempotent with backoff; a checksum mismatch withholds the "synced" mark and surfaces an alert.
- **FR-E4** The sidecar never uploads while the runtime may still append to that segment (seal is a handshake, not a guess).
- **FR-E5** The bucket holds a per-run manifest listing segments in order, uploaded last.

### F. Control tower
- **FR-F1** Registration API: agents, plugins, manifests, grants, quotas.
- **FR-F2** Policy API: per-client allow/deny, quotas, global kill-switch; publishes `policy_version`.
- **FR-F3** Heartbeats and low-volume summary events (not the full stream) so the tower stays off the hot path.
- **FR-F4** A CLI to do all of the above, usable in a demo without a browser.
- **FR-F5** Tower unreachability must degrade **closed** for new grants and **open** for already-granted calls within a stated grace window (owner decision, see Q3).

### G. Runtime / demo surface
- **FR-G1** Agent config declares plugins + grants; a run executes a scripted sequence of mediated calls.
- **FR-G2** Two agents, two plugins, three clients (A allowed, B restricted, C operator read-only) for the demo.
- **FR-G3** One declarative tool (YAML) runs without code.

## 8. Architecture and process model

```
external client ──(own socket + HMAC)──▶ broker/gatekeeper ──(mediated JSON-RPC)──▶ plugin subprocess
                                              │                                          ▲
control tower (HTTP) ──policy_version──▶      │                          watcher spawns/owns
                                              ▼                                          │
                                     journal (hash-chained, outside plugin reach)        │
                                              │                                   bwrap: RO /, no net,
                                       seal + sidecar ──If-None-Match:*──▶ MinIO        rw only scoped dir
```

Processes: `harness` (agent runtime), `control-plane` (tower API + gatekeeper + watcher), `plugin` (N, supervised), `sync-sidecar`, `minio`. The broker is the only path to mediated effects; the plugin's own socket is not externally reachable.

Wire: newline-delimited JSON-RPC, mirroring DSH's Python SDK convention.

## 9. Gatekeeper test matrix (the primary deliverable)

Oracle column = how pass/fail is judged, independent of our logs.

| ID | Layer | Contract | Oracle |
|---|---|---|---|
| GK-01 | validation | Undeclared/unknown grant → refuses to load | No process, no socket exists |
| GK-02 | validation | Artifact hash mismatch → rejected pre-exec | Exec never happens (audit hook) |
| GK-03 | authz | Default deny: no grants → `write` denied | Denied **and** no bytes in scoped dir |
| GK-04 | authz | Identity comes from transport: B sends `client_id=A` | Denied; A's resources untouched |
| GK-05 | isolation | A writes; B (read-only) cannot read A's region | Bytes exist only under `regions/a`; B's result lacks the secret |
| GK-06 | isolation | Hostile fixture: raw `open(/etc/passwd)`, `../`, symlink out, `/proc/self/fd` | All fail inside `bwrap`; nothing created outside; no other region visible |
| GK-07 | supervision | Kill plugin → no surviving descendant | Process-group/cgroup probe, not `ppid` |
| GK-08 | telemetry | Plugin cannot append/truncate/forge journal; plugin-supplied `seq` rejected | Journal bytes unchanged except broker records |
| GK-09 | telemetry | Tamper a sealed segment → chain break detected, upload refused | Reader flags `chain_broken`; bucket object unchanged |
| GK-10 | sync | `FAIL_AFTER_SEQ=N` crash → uploaded run reads incomplete | Objects present, no `run_complete`; reader says incomplete |
| GK-11 | sync | Same key re-upload → `412`, never overwritten; retry idempotent | One object, identical bytes |
| GK-12 | sync | Mediated write with known payload hash appears in the uploaded object | Hash + broker `seq` present in bucket bytes |
| GK-13 | authz | Revoke B only → B denied, A still served, same PID alive | PID alive, A's call returns, B denied |
| GK-14 | authz | Global quarantine → in-flight denied within bound, group dead | No mutation after bound; no descendants |

Anti-theater rule: any test whose pass condition is "the broker logged denied" is scored **fail**. Tests emit through the same channel they audit, so completeness claims need an outside effect (file, process, bucket object).

## 10. What this demo will NOT prove (stated up front)

1. **Journal completeness** — we prove ordering, chaining, and immutability of what the broker records; we do not prove every possible side effect was routed through the broker unless `bwrap` truly forbids the alternatives, which GK-06 tests for our plugin shape only.
2. **Bucket authenticity** — objects are verified as intact and non-overwritten, not as authored by us; a compromised component holding bucket credentials could upload a well-formed forgery.
3. **Universal containment** — containment is proven for the fixtures we ship, under `bwrap` on this host, not for arbitrary future plugins.
4. **Multi-host correctness** — revoke and quota semantics are single-host.

## 11. Milestones

| M | Deliverable | Exit criteria |
|---|---|---|
| M0 | Scaffold: uv project, schemas, reference notes from the DSH clone | `uv run pytest` green (empty suite), schema round-trip works |
| M1 | Broker + one plugin subprocess under `bwrap` + validator | GK-01…GK-07 green |
| M2 | Journal, hash chain, sealing, sidecar, MinIO via `docker run` | GK-08…GK-12 green |
| M3 | Tower API, policy versions, per-client rules, revoke | GK-13…GK-14 green |
| M4 | Demo script, declarative tool, README, diagram refresh | End-to-end demo runs twice from clean; suite green |

## 12. Risks

| Risk | Mitigation |
|---|---|
| **Confused deputy** — shared instance leaks one client's state to another (critic #1) | No cached authority in plugin; per-request handle; GK-04/05; interleaved concurrent clients in GK-05 |
| **Theater sandboxing** — static manifest checks don't stop `open("/etc/passwd")` (critic #1) | Kernel enforcement via `bwrap` (verified working here), hostile fixture GK-06 |
| **Kill-switch doesn't reach the data plane** — in-flight write commits after revoke (critic #1) | `policy_version` checked per call; bound stated in GK-14; process-group kill |
| **Watcher can't express per-client rules** — one PID can't be half-killed (critic #1) | Per-client enforcement lives in the broker, not the watcher; GK-13 asserts PID stays alive |
| **Sidecar fabricates confidence** (critic #1) | Oracle-based GK-12; hash of uploaded bytes, not local file |
| **Crashed run looks complete** (critic #2) | Explicit `run_complete`; `FAIL_AFTER_SEQ` hook; GK-10 |
| **Control-plane sprawl before core works** (critic #2) | M1–M2 need no tower; tower is M3 and uses a CLI, not a UI |
| **No compose plugin on this host** (verified) | MinIO via `docker run` + Makefile target; compose optional later |

## 13. Open questions for the owner

- **Q1** Clients are external consumers, but agents also invoke plugins. Are agents a client class with their own rules, or does agent traffic bypass client rules entirely? (This changes the test matrix more than anything else.)
- **Q2** Repo name and location: propose `/home/alex/git/my/telemetry-harness`. Confirm or rename.
- **Q3** When the tower is unreachable: fail closed for new grants and allow already-granted calls for N seconds — what is N, or should it be strict fail-closed?
- **Q4** Does "validation" include running the plugin's own self-test before admission, or only static artifact/manifest checks?
- **Q5** Is `spawn`/`handoff` in v0.1 (critic #2 would cut it)? Owning a child process is the most expensive thing to supervise correctly.

## 14. Appendix — environment verified

- `deepseek-harness` cloned at `master` = `39163dd3` (2026-11-13), 251 MB, shallow.
- `python3 3.14.10`, `uv 0.13.8` (project will pin 3.12+).
- `bwrap` present and functional (`--unshare-net` works) → kernel-level isolation is testable.
- `docker` 29.9.3 daemon reachable (rootless, seccomp); **no** `docker-compose` binary and no compose plugin → `docker run` only.
