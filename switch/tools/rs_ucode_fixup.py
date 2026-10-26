# Same edits as RogueSquadron64Recomp tools/fixup_factor5_ucode.cpp, reimplemented.
import re, sys
def fix(text):
    lines = text.split("\n")
    labels = {m.group(1) for l in lines for m in [re.match(r'^([A-Za-z_]\w*):\s*$', l)] if m}
    def repl(m):
        return m.group(0) if m.group(1) in labels else "return RspExitReason::ImemOverrun;"
    lines = [re.sub(r'(?<![\w])goto\s+(\w+);', repl, l) for l in lines]
    silence = "/* fixup: silence expected boot->main JR */"
    if "factor5_boot(" in text and silence not in text:
        for i, l in enumerate(lines):
            if 'printf("Unhandled jump target 0x%04X in microcode factor5_boot,' in l:
                ind = l[:len(l) - len(l.lstrip(" \t"))]
                lines[i] = "    " + l
                lines.insert(i, ind + "if (jump_target != 0x1080) {  " + silence)
                for j in range(i + 2, len(lines) - 1):
                    if lines[j] == "    return RspExitReason::UnhandledJumpTarget;" and lines[j + 1] == "}":
                        lines.insert(j, "    }"); break
                break
    cap = "/* fixup: musyx runaway cap */"
    musyx = "musyx_audio(" in text
    if musyx and cap not in text:
        i = lines.index("    RSP rsp{};"); lines.insert(i + 1, "    long rs64_iter = 0;  " + cap)
        i = lines.index("do_indirect_jump:")
        lines[i + 1:i + 1] = ["    if (jump_target == 0) {  " + cap, "        return RspExitReason::Broke;", "    }",
            "    if (++rs64_iter > (1L<<20)) {  " + cap, "        static int rs64_musyx_cap = 0;",
            "        if (++rs64_musyx_cap <= 4) {",
            '            fprintf(stderr, "[musyx-cap] fired #%d, last jump_target=0x%04X\\n", rs64_musyx_cap, jump_target);',
            "            fflush(stderr);", "        }", "        return RspExitReason::Broke;", "    }"]
    unblock = "/* no-voice unblock"
    if musyx and unblock not in text:
        for i, l in enumerate(lines):
            if l.strip() == "goto L_13EC;":
                ind = l[:len(l) - len(l.lstrip(" \t"))]
                lines[i] = ind + "if (r29 != 0) goto L_13EC;  " + unblock + ": r29==0 (null voice) exits instead of spinning */"; break
    out = "\n".join(lines)
    if "#include <cstdio>" not in out: out = "#include <cstdio>\n" + out
    return out
for p in sys.argv[1:]:
    t = open(p).read(); n = fix(t)
    if n != t: open(p, "w").write(n); print("fixed", p)
