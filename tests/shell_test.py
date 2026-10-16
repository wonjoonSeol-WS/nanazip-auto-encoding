# Drives NanaZipAutoCP.dll through tests\test_drag.exe with a stand-in for Expand-ArchiveAuto.ps1
# that records how it was called. Needs src\build.cmd to have run. Replaces the values of
# HKCU\Software\NanaZipAutoCP while it runs and restores them afterwards.
# usage: python -I tests/shell_test.py
import json, os, subprocess, sys, tempfile, time, winreg

sys.stdout.reconfigure(encoding="utf-8", errors="backslashreplace")
here = os.path.dirname(os.path.abspath(__file__))
dll = os.path.normpath(os.path.join(here, "..", "NanaZipAutoCP.dll"))
driver = os.path.join(here, "test_drag.exe")
KEY = r"Software\NanaZipAutoCP"

STAND_IN = r"""[CmdletBinding(PositionalBinding = $false)]
param([Parameter(ValueFromRemainingArguments = $true)][string[]]$Path, [string]$Mode, [string]$OutDir)
$out = $null
if ($OutDir) { $out = (Resolve-Path -LiteralPath $OutDir).ProviderPath }
$line = [pscustomobject]@{ Path = @($Path); Mode = $Mode; OutDir = $out } | ConvertTo-Json -Compress
[IO.File]::AppendAllText((Join-Path $PSScriptRoot 'calls.jsonl'), $line + "`n")
"""


def read_settings():
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, KEY) as k:
            values, i = {}, 0
            while True:
                try:
                    name, value, kind = winreg.EnumValue(k, i)
                except OSError:
                    return values
                values[name] = (value, kind)
                i += 1
    except FileNotFoundError:
        return None


# Values only: the installer keeps its Capabilities subkey under the same key
def write_settings(values):
    if values is None:
        try:
            winreg.DeleteKey(winreg.HKEY_CURRENT_USER, KEY)
        except FileNotFoundError:
            pass
        return
    old = read_settings() or {}
    with winreg.CreateKey(winreg.HKEY_CURRENT_USER, KEY) as k:
        for name in old.keys() - values.keys():
            winreg.DeleteValue(k, name)
        for name, (value, kind) in values.items():
            winreg.SetValueEx(k, name, 0, kind, value)


def run(mode, folder, command, *files):
    r = subprocess.run([driver, dll, mode, folder, str(command), *files], capture_output=True, encoding="utf-8", errors="replace", timeout=60)
    return r.stdout


def menu_labels(out):
    return [line.split(": ", 1)[1] for line in out.splitlines() if line.startswith("  id ")]


failures = 0


def check(ok, label, detail=""):
    global failures
    failures += not ok
    print(("PASS " if ok else "FAIL ") + label + ("" if ok else "\n   " + detail))


def calls(path, count):
    deadline = time.time() + 60
    while time.time() < deadline:
        if os.path.exists(path):
            lines = open(path, encoding="utf-8").read().splitlines()
            if len(lines) >= count:
                return [json.loads(l) for l in lines]
        time.sleep(0.2)
    return []


backup = read_settings()
try:
    with tempfile.TemporaryDirectory(prefix="nanazip-shell-") as d:
        stand_in = os.path.join(d, "stand-in.ps1")
        open(stand_in, "w", encoding="utf-8").write(STAND_IN)
        target = os.path.join(d, "target")
        os.makedirs(target)
        os.makedirs(os.path.join(d, "dir.zip"))
        a, b, note = (os.path.join(d, n) for n in ("a.zip", "b.ZIP", "note.txt"))
        for f in (a, b, note):
            open(f, "wb").close()
        settings = {"Script": (stand_in, winreg.REG_SZ), "Extensions": (".zip;.7z", winreg.REG_SZ),
                    "LabelSmart": ("Smart label", winreg.REG_SZ), "LabelFolder": ("Folder label", winreg.REG_SZ)}
        write_settings(settings)
        log = os.path.join(d, "calls.jsonl")

        out = run("drag", target, -1, a)
        check(menu_labels(out) == ["Smart label", "Folder label"], "drag: two items for an archive", out)
        out = run("click", d, -1, b)
        check(menu_labels(out) == ["Smart label", "Folder label"], "click: extensions match case-insensitively", out)
        check(menu_labels(run("drag", target, -1, a, note)) == [], "drag: no menu when a non-archive is selected too")
        check(menu_labels(run("drag", target, -1, os.path.join(d, "dir.zip"))) == [], "drag: no menu for a folder named like an archive")
        check(menu_labels(run("drag", a, -1, b)) == [], "drag: no menu when the drop target is a file")

        out = run("drag", target, 0, a, b)
        got = calls(log, 1)
        check(len(got) == 1 and got[0] == {"Path": [a, b], "Mode": "Smart", "OutDir": target},
              "drag: one run with every archive, the mode and the drop target", f"{out}\n   {got}")
        os.remove(log)
        out = run("click", d, 1, a)
        got = calls(log, 1)
        check(len(got) == 1 and got[0] == {"Path": [a], "Mode": "Folder", "OutDir": None},
              "click: no -OutDir, so results go next to the archive", f"{out}\n   {got}")
        os.remove(log)
        root = os.path.splitdrive(d)[0] + "\\"
        out = run("drag", root, 0, a)
        got = calls(log, 1)
        check(len(got) == 1 and got[0]["OutDir"] == root, "drag: a drive root survives the quoting", f"{out}\n   {got}")

        write_settings({**settings, "Script": (os.path.join(d, "missing.ps1"), winreg.REG_SZ)})
        check(menu_labels(run("drag", target, -1, a)) == [], "no menu when the script is missing")
        write_settings({**settings, "Extensions": (".7z", winreg.REG_SZ)})
        check(menu_labels(run("drag", target, -1, a)) == [], "no menu for extensions not in the list")
finally:
    write_settings(backup)

print("failures:", failures)
sys.exit(1 if failures else 0)
