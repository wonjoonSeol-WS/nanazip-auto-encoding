# Builds fixture archives and checks the code page Expand-ArchiveAuto.ps1 detects for each.
# Without --detect-only it also extracts fixtures with NanaZipC and compares the resulting folder
# trees, which needs NanaZip installed.
# usage: python -I tests/harness.py [--detect-only]
import os, subprocess, sys, tarfile, tempfile

sys.stdout.reconfigure(encoding="utf-8", errors="backslashreplace")  # -I ignores PYTHONIOENCODING
here = os.path.dirname(os.path.abspath(__file__))
script = os.path.normpath(os.path.join(here, "..", "Expand-ArchiveAuto.ps1"))
mkzip = os.path.join(here, "mkzip.py")
detect_only = "--detect-only" in sys.argv[1:]

# (file name, encoding, mkzip options, entry names, expected -DetectOnly output)
DETECT = [
    ("jp_kana.zip", "cp932", [], ["テスト/日本語ファイル.txt", "テスト/ソフト説明書.txt", "表示/能力.txt"], "932"),
    ("jp_also_valid_cp949.zip", "cp932", [], ["テスト/テスト.txt"], "932"),
    ("jp_kanji_only.zip", "cp932", [], ["説明書/資料.txt", "画像/背景.png"], "932"),
    ("jp_also_valid_utf8.zip", "cp932", [], ["凜ァ.txt"], "932"),
    # 資料 + ED40 (纊, an IBM extension character .NET does not decode) + .txt
    ("jp_ibm_extension.zip", "cp932", [], ["x:" + "資料".encode("cp932").hex() + "ed40" + b".txt".hex()], "932"),
    ("ko.zip", "cp949", [], ["한글/보고서.txt", "사진/여행 사진.jpg"], "949"),
    ("ko_also_valid_cp932.zip", "cp949", [], ["한글/문서.txt"], "949"),
    ("ko_also_valid_utf8.zip", "cp949", [], ["캡처/a.png"], "949"),
    # E0A1 is 爍 in Shift-JIS and a hanja in CP949, so both score the same
    ("tie_goes_to_cp949.zip", "cp932", [], ["爍.txt"], "949"),
    ("utf8_flag.zip", "utf-8", [], ["u:テスト/日本語.txt", "u:한글/문서.txt"], "default"),
    ("utf8_no_flag.zip", "utf-8", [], ["テスト/日本語.txt", "한글/문서.txt"], "65001"),
    ("utf8_no_flag_macos_nfd.zip", "utf-8", [], ["café.txt"], "65001"),
    ("ascii.zip", "ascii", [], ["docs/readme.txt", "a.txt"], "default"),
    ("unicode_path_field.zip", "cp932", [], ["p:テスト/日本語.txt", "p:表示/能力.txt"], "default"),
    ("stale_unicode_path_field.zip", "cp932", [], ["s:テスト/日本語.txt"], "932"),
    ("mixed_flags.zip", "cp932", [], ["日本語/テスト.txt", "u:한글/문서.txt"], "932"),
    ("western_cp850.zip", "cp850", [], ["café.txt", "Größe.txt"], "default"),
    ("zip64.zip", "cp932", ["--zip64"], ["テスト/日本語.txt"], "932"),
    ("sfx_stub.zip", "cp932", ["--sfx"], ["テスト/日本語.txt"], "932"),
    ("sfx_stub_zip64.zip", "cp932", ["--sfx", "--zip64"], ["テスト/日本語.txt"], "932"),
    ("comment_with_end_signature.zip", "cp932", ["--fake-eocd-comment"], ["テスト/日本語.txt"], "932"),
    ("corrupt_central_directory.zip", "cp932", ["--corrupt"], ["テスト/日本語.txt"], "default"),
    # Detection only reads .zip files; other formats store Unicode names or reject -mcp
    ("zip_content_named.7z", "cp932", [], ["テスト/日本語.txt"], "default"),
]

# Each case runs the script once on all its archives and lists what ends up in the folder,
# folders with a trailing slash. "pre" creates files or folders before the run.
EXTRACT = [
    dict(name="smart_several_top_items", zips=[("jp.zip", "cp932", ["テスト/日本語ファイル.txt", "表示/能力.txt"])],
         want=["jp/", "jp/テスト/", "jp/テスト/日本語ファイル.txt", "jp/表示/", "jp/表示/能力.txt"]),
    dict(name="smart_single_top_folder", zips=[("one.zip", "cp932", ["フォルダ/a.txt", "フォルダ/サブ/ソ.txt"])],
         want=["フォルダ/", "フォルダ/a.txt", "フォルダ/サブ/", "フォルダ/サブ/ソ.txt"]),
    dict(name="smart_single_file", zips=[("f.zip", "cp932", ["ソフト説明書.txt"])], want=["ソフト説明書.txt"]),
    dict(name="smart_top_folder_named_like_archive", zips=[("same.zip", "cp932", ["same/ソフト.txt"])],
         want=["same/", "same/ソフト.txt"]),
    dict(name="smart_top_folder_name_taken", zips=[("one.zip", "cp932", ["フォルダ/a.txt"])], pre=["フォルダ/keep.txt"],
         want=["フォルダ/", "フォルダ/keep.txt", "one/", "one/フォルダ/", "one/フォルダ/a.txt"]),
    dict(name="folder_name_taken_gets_number", zips=[("jp.zip", "cp932", ["テスト/a.txt"])], args=["-Mode", "Folder"],
         pre=["jp/keep.txt"], want=["jp/", "jp/keep.txt", "jp (2)/", "jp (2)/テスト/", "jp (2)/テスト/a.txt"]),
    dict(name="folder_even_for_single_top_folder", zips=[("one.zip", "cp932", ["フォルダ/a.txt"])], args=["-Mode", "Folder"],
         want=["one/", "one/フォルダ/", "one/フォルダ/a.txt"]),
    dict(name="outdir", zips=[("jp.zip", "cp932", ["テスト/a.txt", "表示/b.txt"])], args=["-OutDir", "out"], pre=["out/"],
         want=["out/", "out/jp/", "out/jp/テスト/", "out/jp/テスト/a.txt", "out/jp/表示/", "out/jp/表示/b.txt"]),
    dict(name="empty_archive_creates_nothing", zips=[("empty.zip", "cp932", [])], want=[]),
    dict(name="tar_gz_unpacks_the_tar_too", tars=[("docs.tar.gz", ["docs/a.txt", "docs/b.txt"])],
         want=["docs/", "docs/a.txt", "docs/b.txt"]),
    dict(name="several_archives_in_one_run", zips=[("a.zip", "cp932", ["テスト/x.txt", "表示/y.txt"]),
                                                    ("b.zip", "cp949", ["폴더/z.txt"])],
         want=["a/", "a/テスト/", "a/テスト/x.txt", "a/表示/", "a/表示/y.txt", "폴더/", "폴더/z.txt"]),
    dict(name="korean_names", zips=[("ko.zip", "cp949", ["한글/보고서.txt", "사진/여행 사진.jpg"])],
         want=["ko/", "ko/사진/", "ko/사진/여행 사진.jpg", "ko/한글/", "ko/한글/보고서.txt"]),
    dict(name="mixed_flags", zips=[("mixed.zip", "cp932", ["日本語/テスト.txt", "u:한글/문서.txt"])],
         want=["mixed/", "mixed/日本語/", "mixed/日本語/テスト.txt", "mixed/한글/", "mixed/한글/문서.txt"]),
    dict(name="unicode_path_field", zips=[("up.zip", "cp932", ["p:テスト/日本語.txt", "p:表示/能力.txt"])],
         want=["up/", "up/テスト/", "up/テスト/日本語.txt", "up/表示/", "up/表示/能力.txt"]),
]


def ps(*args):
    return subprocess.run(["powershell", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-File", script, *args],
                          capture_output=True, stdin=subprocess.DEVNULL, timeout=600, encoding="utf-8", errors="replace")


def make_zip(path, encoding, options, names):
    subprocess.run([sys.executable, "-I", mkzip, path, encoding, *options, *names], check=True)


def make_tar_gz(path, names):
    with tarfile.open(path, "w:gz") as tar:
        for n in names:
            src = path + ".member"
            with open(src, "wb") as f:
                f.write(b"hello")
            tar.add(src, arcname=n)
            os.remove(src)


def tree(root, skip):
    out = []
    for dp, dirs, files in os.walk(root):
        rel = os.path.relpath(dp, root).replace("\\", "/")
        prefix = "" if rel == "." else rel + "/"
        out += [prefix + d + "/" for d in dirs] + [prefix + f for f in files if prefix + f not in skip]
    return sorted(out)


failures = 0


def report(ok, label, got, want, r=None):
    global failures
    failures += not ok
    print(("PASS " if ok else "FAIL ") + label)
    if not ok:
        print("   got :", got)
        print("   want:", want)
        if r is not None:
            print("   rc:", r.returncode, r.stdout[-400:], r.stderr[-400:])


with tempfile.TemporaryDirectory(prefix="nanazip-auto-") as root:
    detect_dir = os.path.join(root, "detect")
    os.makedirs(detect_dir)
    paths = []
    for file, encoding, options, names, _ in DETECT:
        paths.append(os.path.join(detect_dir, file))
        make_zip(paths[-1], encoding, options, names)
    r = ps("-DetectOnly", *paths)
    got = dict(reversed(line.split("\t", 1)) for line in r.stdout.splitlines() if "\t" in line)
    for path, (file, _, _, _, want) in zip(paths, DETECT):
        report(got.get(path) == want and r.returncode == 0, "detect " + file, got.get(path), want, r)

    if not detect_only:
        for case in EXTRACT:
            d = os.path.join(root, case["name"])
            os.makedirs(d)
            for p in case.get("pre", []):
                target = os.path.join(d, p)
                if p.endswith("/"):
                    os.makedirs(target, exist_ok=True)
                else:
                    os.makedirs(os.path.dirname(target), exist_ok=True)
                    open(target, "wb").close()
            archives = []
            for file, encoding, names in case.get("zips", []):
                archives.append(os.path.join(d, file))
                make_zip(archives[-1], encoding, [], names)
            for file, names in case.get("tars", []):
                archives.append(os.path.join(d, file))
                make_tar_gz(archives[-1], names)
            args = [os.path.join(d, a) if prev == "-OutDir" else a for prev, a in zip([None] + case.get("args", []), case.get("args", []))]
            r = ps("-Console", *args, *archives)
            got = tree(d, {os.path.basename(a) for a in archives})
            report(got == sorted(case["want"]) and r.returncode == 0, "extract " + case["name"], got, sorted(case["want"]), r)

print("failures:", failures)
sys.exit(1 if failures else 0)
