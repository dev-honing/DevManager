#!/usr/bin/env python3
"""
Semantic regression diff between the C++ scanner output and the PowerShell
v4.1 inventory.json. Ignores key order, schemaVersion, timestamps, and the
producedBy/snapshotVersion banner.

usage: python scripts/diff-inventory.py <powershell inventory.json> <cpp inventory.json>
exit 0 = equivalent, 1 = differences found, 2 = usage/read error
"""
import json
import sys


def load(path):
    with open(path, "r", encoding="utf-8-sig") as f:
        return json.load(f)


def loc_key(l):
    return l.get("path", "").lower().replace("/", "\\")


def norm_skills(inv):
    out = {}
    for s in inv.get("discovery", {}).get("skills", []):
        locs = {}
        for l in s.get("locations", []):
            link = l.get("link", {}) or {}
            git = l.get("git", {}) or {}
            locs[loc_key(l)] = {
                "classification": l.get("classification"),
                "isLink": bool(link.get("isLink")),
                "linkType": link.get("linkType"),
                "target": (link.get("target") or "").lower().replace("/", "\\"),
                "gitRemote": git.get("remote"),
                "gitCommit": git.get("commit"),
                "hasSkillMd": l.get("hasSkillMd"),
            }
        out[s["name"].lower()] = locs
    return out


def norm_plugins(inv):
    out = {}
    for p in inv.get("discovery", {}).get("plugins", []):
        out[p["name"].lower()] = sorted(loc_key(l) for l in p.get("locations", []))
    return out


def norm_pkgs(inv):
    return {
        (p["manager"], p["name"].lower()): p["version"]
        for p in inv.get("discovery", {}).get("globalPackages", [])
    }


def is_masked(v):
    return v == "***" or ("..." in v and len(v) <= 12)


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    try:
        ps, cpp = load(sys.argv[1]), load(sys.argv[2])
    except Exception as e:  # noqa: BLE001
        print(f"read error: {e}")
        return 2

    diffs = []

    # tools  (toolPaths is a UI-only convenience field, not compared)
    for k in sorted(set(ps.get("tools", {})) | set(cpp.get("tools", {}))):
        a, b = ps.get("tools", {}).get(k), cpp.get("tools", {}).get(k)
        if (a or "") != (b or ""):
            diffs.append(f"tools.{k}: ps={a!r} cpp={b!r}")

    for k in ("qt", "visualStudio"):
        if (ps.get(k) or "") != (cpp.get(k) or ""):
            diffs.append(f"{k}: ps={ps.get(k)!r} cpp={cpp.get(k)!r}")

    # wsl
    pw = {d["name"]: (d.get("state"), d.get("version")) for d in ps.get("wsl", {}).get("distributions", [])}
    cw = {d["name"]: (d.get("state"), d.get("version")) for d in cpp.get("wsl", {}).get("distributions", [])}
    if set(pw) != set(cw):
        diffs.append(f"wsl distros: ps={sorted(pw)} cpp={sorted(cw)}")
    for name in set(pw) & set(cw):
        if pw[name][1] != cw[name][1]:
            diffs.append(f"wsl.{name}.version: ps={pw[name][1]} cpp={cw[name][1]}")

    # discovery roots (as sets, case-insensitive)
    for key in ("skillRoots", "pluginRoots"):
        a = {x.lower() for x in ps.get("discovery", {}).get(key, [])}
        b = {x.lower() for x in cpp.get("discovery", {}).get(key, [])}
        if a != b:
            diffs.append(f"discovery.{key}: only-ps={sorted(a - b)} only-cpp={sorted(b - a)}")

    # skills
    ss, cs = norm_skills(ps), norm_skills(cpp)
    if set(ss) != set(cs):
        diffs.append(f"skills: only-ps={sorted(set(ss) - set(cs))} only-cpp={sorted(set(cs) - set(ss))}")
    for name in set(ss) & set(cs):
        if set(ss[name]) != set(cs[name]):
            diffs.append(f"skill {name}: location set differs")
            continue
        for lk in ss[name]:
            if ss[name][lk] != cs[name][lk]:
                diffs.append(f"skill {name} @ {lk}:\n    ps ={ss[name][lk]}\n    cpp={cs[name][lk]}")

    # plugins
    sp, cp = norm_plugins(ps), norm_plugins(cpp)
    if sp != cp:
        diffs.append(f"plugins: ps={sp} cpp={cp}")

    # packages
    spk, cpk = norm_pkgs(ps), norm_pkgs(cpp)
    if set(spk) != set(cpk):
        only_ps = sorted(spk.keys() - cpk.keys())
        only_cpp = sorted(cpk.keys() - spk.keys())
        diffs.append(f"packages: only-ps={only_ps[:10]}... only-cpp={only_cpp[:10]}...")
    for key in spk.keys() & cpk.keys():
        if spk[key] != cpk[key]:
            diffs.append(f"package {key}: ps={spk[key]} cpp={cpk[key]}")

    # env (compare key presence + masked/not, not raw secret values)
    se, ce = ps.get("env", {}), cpp.get("env", {})
    if set(se) != set(ce):
        diffs.append(f"env keys: only-ps={sorted(set(se) - set(ce))} only-cpp={sorted(set(ce) - set(se))}")
    for k in set(se) & set(ce):
        if is_masked(str(se[k])) != is_masked(str(ce[k])):
            diffs.append(f"env.{k}: masking differs ps={se[k]!r} cpp={ce[k]!r}")

    if not diffs:
        print("EQUIVALENT - no semantic differences")
        return 0
    print(f"{len(diffs)} difference(s):\n")
    for d in diffs:
        print("  - " + d)
    return 1


if __name__ == "__main__":
    sys.exit(main())
