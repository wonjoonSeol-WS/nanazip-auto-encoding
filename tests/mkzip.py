# Builds a stored (uncompressed) ZIP with raw entry names, for the encoding tests.
# usage: mkzip.py OUT ENCODING [--zip64] [--sfx] [--fake-eocd-comment] [--corrupt] [NAME ...]
# NAME is a legacy name in ENCODING, as old Windows zippers write it, or one of:
#   u:NAME  UTF-8 name with general purpose bit 11 set, as modern zippers write it
#   p:NAME  legacy name plus an Info-ZIP Unicode Path field (0x7075) holding the UTF-8 name
#   s:NAME  like p: but the field's CRC does not match the stored name (a stale field)
#   x:HEX   raw name bytes in hex
import argparse, struct, zlib

ap = argparse.ArgumentParser()
ap.add_argument("out")
ap.add_argument("encoding")
ap.add_argument("--zip64", action="store_true", help="ZIP64 end records, with only the central directory offset overflowed")
ap.add_argument("--sfx", action="store_true", help="prepend a 1 KiB stub without adjusting any offset")
ap.add_argument("--fake-eocd-comment", action="store_true", help="archive comment that contains an end record signature")
ap.add_argument("--corrupt", action="store_true", help="first central directory entry claims a name longer than the directory")
ap.add_argument("names", nargs="*")
a = ap.parse_args()

data = b"hello"
crc = zlib.crc32(data)
local, central = b"", b""
for i, spec in enumerate(a.names):
    kind, _, text = spec.partition(":") if spec[1:2] == ":" else ("", "", spec)
    gp, extra = 0, b""
    if kind == "u":
        name, gp = text.encode("utf-8"), 0x0800
    elif kind == "x":
        name = bytes.fromhex(text)
    else:
        name = text.encode(a.encoding)
    if kind in ("p", "s"):
        name_crc = zlib.crc32(name) ^ (0xFFFFFFFF if kind == "s" else 0)
        body = struct.pack("<BI", 1, name_crc) + text.encode("utf-8")
        extra = struct.pack("<HH", 0x7075, len(body)) + body
    name_len = 0xFFFF if a.corrupt and i == 0 else len(name)
    offset = len(local)
    local += struct.pack("<IHHHHHIIIHH", 0x04034B50, 20, gp, 0, 0, 0, crc, len(data), len(data), len(name), len(extra)) + name + extra + data
    central += struct.pack("<IHHHHHHIIIHHHHHII", 0x02014B50, 20, 20, gp, 0, 0, 0, crc, len(data), len(data),
                           name_len, len(extra), 0, 0, 0, 0, offset) + name + extra

count, cd_offset, cd_size = len(a.names), len(local), len(central)
tail = b""
if a.zip64:
    zip64_offset = cd_offset + cd_size
    tail += struct.pack("<IQHHIIQQQQ", 0x06064B50, 44, 45, 45, 0, 0, count, count, cd_size, cd_offset)
    tail += struct.pack("<IIQI", 0x07064B50, 0, zip64_offset, 1)
    cd_offset = 0xFFFFFFFF
comment = b"PK\x05\x06" + b"\0" * 18 + b"not the end record" if a.fake_eocd_comment else b""
tail += struct.pack("<IHHHHIIH", 0x06054B50, 0, 0, count, count, cd_size, cd_offset, len(comment)) + comment
with open(a.out, "wb") as f:
    if a.sfx:
        f.write(b"MZ" + b"\0" * 1022)
    f.write(local + central + tail)
