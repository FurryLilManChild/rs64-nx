# Python port of RogueSquadron64Recomp tools/state/gen_state_table.cpp (output only; validation skipped).
import sys, tomllib
st = tomllib.load(open(sys.argv[1], "rb")).get("state", [])
idx = {s["id"]: i for i, s in enumerate(st)}
def depth(s):
    d = 0; cur = s
    while cur.get("parent", ""):
        d += 1
        if cur["parent"] not in idx: break
        cur = st[idx[cur["parent"]]]
    return d
def c(s): return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'
OPS = {"eq": "RS_OP_EQ", "ne": "RS_OP_NE", "range": "RS_OP_RANGE", "flag": "RS_OP_FLAG"}
SLOTS = {"boot_level": ("RS_SLOT_BOOT_LEVEL", ["level", "craft"]), "menu_screen": ("RS_SLOT_MENU_SCREEN", ["screen"]), "demo": ("RS_SLOT_DEMO", ["demo"])}
preds, fps, rf, rows = [], [], [], []
u = lambda v: v & 0xFFFFFFFF
for s in st:
    po = len(preds)
    for p in s.get("classify", []):
        preds.append("  { 0x%08Xu, 0x%08Xu, %s, 0x%08Xu, 0x%08Xu }," % (u(p["addr"]), u(p["mask"]), OPS[p["op"]], u(p["value"]), u(p.get("value_hi", 0))))
    fo = len(fps); slot = "RS_SLOT_NONE"
    f = s.get("force")
    if f:
        slot, keys = SLOTS[f["slot"]]
        for k in keys: fps.append("  %d," % f.get("params", {}).get(k, -1))
    ro = len(rf)
    for r in s.get("reachable_from", []): rf.append("  " + c(r) + ",")
    parent = idx.get(s.get("parent", ""), -1)
    rows.append("  { %s, %s, %d, %d, %d, %d, %s, %d, %d, %d, %d }," % (c(s["id"]), c(s["name"]), parent, depth(s), po, len(s.get("classify", [])), slot, fo, len(fps) - fo, ro, len(s.get("reachable_from", []))))
j = lambda v, e: "\n".join(v) if v else e
t = "// GENERATED from state_model.toml. Do not edit.\n\n"
t += "static const RsPredicate g_state_predicates[] = {\n" + j(preds, "  {0,0,0,0,0},") + "\n};\n\n"
t += "static const int32_t g_state_force_params[] = {\n" + j(fps, "  0,") + "\n};\n\n"
t += "static const char* const g_state_reachable[] = {\n" + j(rf, "  0,") + "\n};\n\n"
t += "static const RsState g_state_table[] = {\n" + j(rows, "") + "\n};\n\n"
t += "#define RS_STATE_COUNT %d\n" % len(st)
open(sys.argv[2], "w").write(t); print("wrote", len(st), "states")
