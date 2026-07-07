#!/usr/bin/env python3
"""Patch parametrow w ~/.config/FiSynth.settings (filterState).

Format: PropertiesFile XML -> VALUE name="filterState" val=<juce MemoryBlock
base64 (wlasna tablica)> -> copyXmlToBinary(magic 0x21324356 LE, u32 len,
XML single-line, NUL) -> XML APVTS.

Uzycie: patch_state.py id=value [id=value ...]
Bez argumentow: dekoduje i wypisuje XML stanu.
"""
import re
import sys

SETTINGS = "/home/mszramka/.config/FiSynth.settings"
TABLE = ".ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+"


def b64_decode(s: str) -> bytes:
    dot = s.index(".")
    size = int(s[:dot])
    val, nbits, out = 0, 0, bytearray()
    for ch in s[dot + 1:]:
        val |= TABLE.index(ch) << nbits
        nbits += 6
        while nbits >= 8:
            out.append(val & 0xFF)
            val >>= 8
            nbits -= 8
    return bytes(out[:size])


def b64_encode(data: bytes) -> str:
    num_chars = (len(data) * 8 + 5) // 6
    val = int.from_bytes(data, "little")
    return f"{len(data)}." + "".join(
        TABLE[(val >> (6 * i)) & 63] for i in range(num_chars))


def unwrap_xml(blob: bytes) -> str:
    magic = int.from_bytes(blob[0:4], "little")
    assert magic == 0x21324356, hex(magic)
    length = int.from_bytes(blob[4:8], "little")
    return blob[8:8 + length].rstrip(b"\x00").decode("utf-8")


def wrap_xml(xml: str) -> bytes:
    body = xml.encode("utf-8") + b"\x00"
    return (0x21324356).to_bytes(4, "little") + len(body).to_bytes(4, "little") + body


def set_param(xml: str, pid: str, value: str) -> str:
    pat = re.compile(r'(<PARAM id="%s" value=")[^"]*(")' % re.escape(pid))
    if pat.search(xml):
        return pat.sub(lambda m: m.group(1) + value + m.group(2), xml)
    # brak wpisu (stary stan) -> dopisz przed pierwszym dzieckiem nie-PARAM
    ins = '<PARAM id="%s" value="%s"/>' % (pid, value)
    for marker in ("<ENVELOPE", "<GATE", "</PARAMETERS>"):
        idx = xml.find(marker)
        if idx >= 0:
            return xml[:idx] + ins + xml[idx:]
    raise RuntimeError("nie znalazlem miejsca na insert")


def main():
    with open(SETTINGS, encoding="utf-8") as f:
        settings = f.read()

    m = re.search(r'name="filterState" val="([^"]*)"', settings)
    assert m, "brak filterState w settings"
    blob = b64_decode(m.group(1))
    xml = unwrap_xml(blob)

    # sanity roundtrip: dekodowanie i kodowanie musza byc odwrotne
    assert b64_decode(b64_encode(blob)) == blob
    assert unwrap_xml(wrap_xml(xml)) == xml

    if len(sys.argv) < 2:
        print(xml)
        return

    for arg in sys.argv[1:]:
        pid, value = arg.split("=", 1)
        xml = set_param(xml, pid, value)

    new_val = b64_encode(wrap_xml(xml))
    settings = settings.replace(m.group(1), new_val)
    with open(SETTINGS, "w", encoding="utf-8") as f:
        f.write(settings)
    print("OK, spatchowano:", ", ".join(sys.argv[1:]))


if __name__ == "__main__":
    main()
