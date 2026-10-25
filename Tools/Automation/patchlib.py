# patch(path, [(old, new), ...]) - exact replacements; keeps BOM; files are written with CRLF like the rest of the project
def patch(p, pairs):
    raw = open(p, 'rb').read()
    bom = raw[:3] == b'\xef\xbb\xbf'
    s = raw.decode('utf-8-sig').replace('\r\n', '\n')
    for a, b in pairs:
        assert a in s, (p, a[:80])
        s = s.replace(a, b)
    open(p, 'w', encoding='utf-8-sig' if bom else 'utf-8', newline='\r\n').write(s)
