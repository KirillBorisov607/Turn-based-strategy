"""Game data for the shorts, read from the game's own sources: heroes, abilities, enemies, relics, events.

Usage: python shorts_data.py  ->  C:/ProjectsUE/_logs/game_data.json
"""
import json
import re

SRC = r"C:/ProjectsUE/Turn-based-strategy/The_battle_code/Plugins/KeepersGame/Source/KeepersGame/Private/"
STATS = json.load(open(r"C:/ProjectsUE/_logs/unit_stats.json", encoding="utf-8"))
TEXT = r'TEXT\("((?:[^"\\]|\\.)*)"\)'


def read(name):
    return open(SRC + name, encoding="utf-8-sig").read()


def spells():
    src = read("KeepersHeroes.cpp")
    out = {}
    for m in re.finditer(r'S\(' + TEXT + r', ' + TEXT + r', ' + TEXT + r', ' + TEXT, src):
        out[m.group(1)] = {"name": m.group(2), "text": m.group(3), "numbers": m.group(4)}
    return out


def heroes():
    src = read("KeepersHeroes.cpp")
    out = []
    for var in ("Warrior", "Ranger", "Priest", "Mage", "Rogue", "Barbarian"):
        def field(f):
            m = re.search(var + r'\.' + f + r' = T\(' + TEXT + r'\)', src)
            return m.group(1) if m else ""
        sp = re.search(var + r'\.Spells = \{([^}]*)\}', src).group(1)
        hp = re.search(var + r'\.Hp = (\d+)', src)
        mp = re.search(var + r'\.Mp = (\d+)', src)
        ap = re.search(var + r'\.Ap = (\d+)', src)
        stat = STATS.get(var, {})
        looks = re.search(var + r'\.Looks = \{(.*?)\};', src, re.S).group(1)
        out.append({
            "id": var, "name": field("Name"), "class": field("Class"), "role": field("Role"), "text": field("Description"),
            "unlock": field("Unlock"), "spells": re.findall(TEXT, sp),
            "hp": stat.get("hp", int(hp.group(1)) if hp else 0), "mp": stat.get("mp", int(mp.group(1)) if mp else 0),
            "ap": stat.get("ap", int(ap.group(1)) if ap else 0),
            "looks": re.findall(r'MakeLook\(' + TEXT, looks), "playable": var in ("Warrior", "Ranger", "Priest"),
        })
    return out


ENEMIES = [
    {"id": "Slime", "name": "Слизень", "text": "Оставляет лужи — по ним идти вдвое дольше. Погибнув, делится на двух маленьких.", "spells": ["MeleeHit", "SlimeBall"]},
    {"id": "Chicken", "name": "Курица", "text": "Быстрая и громкая: каждые два хода зовёт подмогу.", "spells": ["MeleeHit", "RunChickenRun"]},
    {"id": "Bat", "name": "Летучая мышь", "text": "Летает над водой и пропастями: столкнуть её нельзя.", "spells": ["MeleeHit", "WindGust"]},
    {"id": "Bandit", "name": "Разбойник", "text": "Наёмник кур. Идёт к ближайшему герою и рубит.", "spells": ["SwordSlash"]},
    {"id": "BanditArcher", "name": "Стрелок", "text": "Наёмник кур. Держится подальше и стреляет.", "spells": ["ShortRangeArrow"]},
    {"id": "Brute", "name": "Громила", "text": "Наёмник кур. Медленный, крепкий и любит сталкивать героев в воду.", "spells": ["SwordSlash", "ShieldBash"]},
]


def enemies():
    for e in ENEMIES:
        s = STATS.get(e["id"], {})
        e.update(hp=s.get("hp", 0), mp=s.get("mp", 0), ap=s.get("ap", 0))
    return ENEMIES


def relics():
    src = read("KeepersCampaign.cpp")
    return [{"id": m.group(1), "name": m.group(2), "text": m.group(3)}
            for m in re.finditer(r'Make\(' + TEXT + r', ' + TEXT + r', ' + TEXT + r'\)', src)]


def events():
    src = read("KeepersEvents.cpp")
    out = []
    for block in re.finditer(r'\{ ' + TEXT + r',\s*' + TEXT + r',\s*\{(.*?)\n\t\t\t\t\} \}', src, re.S):
        options = []
        for o in re.finditer(r'\{ ' + TEXT + r'\s*, (\d+), (\d+), EEffect::(\w+), (-?\d+), ' + TEXT + r'(?:,\s*EEffect::(\w+), (-?\d+), ' + TEXT + r')?', block.group(3)):
            options.append({"label": o.group(1), "cost": int(o.group(2)), "chance": int(o.group(3)), "effect": o.group(4),
                            "amount": int(o.group(5)), "outcome": o.group(6), "fail_effect": o.group(7) or "",
                            "fail_outcome": o.group(9) or ""})
        out.append({"title": block.group(1), "text": block.group(2), "options": options})
    return out


def main():
    data = {"spells": spells(), "heroes": heroes(), "enemies": enemies(), "relics": relics(), "events": events()}
    json.dump(data, open(r"C:/ProjectsUE/_logs/game_data.json", "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    print({k: len(v) for k, v in data.items()})
    print([len(e["options"]) for e in data["events"]])


if __name__ == "__main__":
    main()
