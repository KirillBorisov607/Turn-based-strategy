"""The shorts of the month: series, titles, footage, infographic panels, captions and tags.

catalog() -> list of dicts for make_shorts.render: slug, series, title, seconds, footage, panel(layer, t), pops,
caption, tags, accent.
"""
from PIL import ImageDraw

from make_shorts import (BLUE, CREAM, DATA, DIM, F, Footage, GOLD, GREEN, INK, PANEL_Y, RED, StillFootage, VID_H, VID_Y, W,
                         bar, big_number, card, countdown, ease_back, ease_out, icon, portrait, spell_icon, text_block, wrap)

TAGS = "#хранителиземель #инди #indiegame #геймдев #gamedev #unrealengine5 #тактика #пошаговаястратегия"
HEROES = {h["id"]: h for h in DATA["heroes"]}
SPELLS = DATA["spells"]

SPELL_TIPS = {
    "SwordSlash": "Бей тех, кто уже ранен: добитый враг не ударит в ответ.",
    "RaiseRock": "Скала посреди прохода — и половина врагов идёт в обход.",
    "ShieldBash": "Толкай к воде: удар щитом у берега заканчивает бой врага.",
    "LongRangeArrow": "Стреляет через скалы и деревья — враг за укрытием не спрячется.",
    "Gust": "Порыв в сторону обрыва стоит трёх выстрелов.",
    "ShortRangeArrow": "Сильнее навесного, но нужна видимость: встань так, чтобы цель была на виду.",
    "Heal": "Лечи того, по кому собираются бить: красные клетки подскажут.",
    "Sprout": "Лес закрывает обзор: лучник за деревом не видит цель.",
    "Roots": "Корни на Громилу — и он пропускает ход у края.",
}

ENEMY_TIPS = {
    "Slime": "Добивай маленьких слизней сразу: двое маленьких — это четыре потом.",
    "Chicken": "Кур бей первыми: каждые два хода они зовут подмогу.",
    "Bat": "Летучих не столкнуть в воду — бери их стрелами.",
    "Bandit": "Подставь ему скалу на пути: он упрямо идёт к ближайшему герою.",
    "BanditArcher": "Лес и скалы закрывают от его стрел — подойди под укрытием.",
    "Brute": "Не стой у воды рядом с ним: удар щитом сбросит тебя.",
}

TIPS = [
    ("Уходи с красных клеток", "Враг заранее показывает, куда ударит. Красная клетка — это обещание удара.",
     ["Шаг в сторону — и удар уходит в пустоту", "Скала на красной клетке закрывает союзника"], "battle", 900, 1200),
    ("Сталкивай в воду", "Порыв ветра и удар щитом толкают врага на клетку. В воду или с обрыва — это конец.",
     ["Ищи врагов у края", "Летучих не столкнёшь"], "b_mercs", 300, 600),
    ("Высота решает", "Сверху бить проще, а враги стараются занять холм раньше тебя.",
     ["Подъём — только на 1 уровень", "Обрыв выше — непроходим"], "battle2", 100, 400),
    ("Лес — твой щит", "Дерево закрывает линию обзора: стрелок не видит цель за лесом.",
     ["Росток Друида растит лес в бою", "По лесу идти вдвое дольше"], "b_bats", 200, 500),
    ("Скала — стена", "Страж поднимает скалу на пустой клетке. Это укрытие и стена одновременно.",
     ["Перекрой мост", "Спрячь Друида за камнем"], "b_chickens", 300, 600),
    ("Корни для громилы", "Корни держат врага на месте весь следующий ход.",
     ["Свяжи того, кто опаснее", "Отведи героев от его удара"], "b_mercs", 600, 900),
    ("Лечи заранее", "Друид лечит тех, по кому собираются бить — посмотри на красные клетки.",
     ["Лечение до 5 здоровья", "Можно лечить себя"], "b_protect", 300, 600),
    ("Не стой у края", "Враги тоже умеют толкать. Громила с щитом ищет героев у воды.",
     ["Отойди на клетку от края", "Держи строй"], "b_slimes", 300, 600),
    ("Обойди вожака", "Вожак держится позади, остальные стерегут его.",
     ["Свали вожака — бой выигран", "Навесной выстрел бьёт через стену"], "obj_leader", 250, 550),
    ("Куры зовут подмогу", "Каждые два хода курица зовёт ещё одну. Медлить нельзя.",
     ["Бей кур первыми", "Колокольчик пастуха их глушит"], "b_chickens", 600, 900),
    ("Слизни делятся", "Погибший слизень делится на двух маленьких по 3 здоровья.",
     ["Добивай малышей сразу", "Лужи слизи замедляют"], "b_slimes", 600, 900),
    ("Ходи в любом порядке", "Щёлкни по другому герою посреди хода — управление перейдёт к нему.",
     ["Сначала толчок, потом выстрел", "Прерванный герой доиграет позже"], "battle", 1160, 1340),
]

OBJECTIVES = [
    ("reach", "Дойди до маяка", "obj_reach", 200, 560, "Маяк горит на самой дальней клетке, до которой можно дойти.",
     "Хотя бы один герой должен встать на маяк.", "Стерегут клетки вокруг маяка."),
    ("leader", "Свали вожака", "obj_leader", 250, 610, "Самый крепкий враг — вожак: +50% здоровья и корона.",
     "Победи вожака — и бой выигран, даже если остальные живы.", "Вожак держится позади, свита стоит рядом с ним."),
    ("survive", "Выстой осаду", "obj_survive", 90, 450, "Продержись несколько ходов — враги отступят.",
     "Тяни время: прячься, лечись, перекрывай проходы.", "Лезут напролом: не берегутся у края, раненые не отходят."),
    ("protect", "Защити курицу", "b_protect", 200, 560, "Курица Пеструшка на твоей стороне. Она не должна погибнуть.",
     "Закрывай её скалами и героями.", "Охотятся за ней в первую очередь."),
    ("all", "Победи всех", "b_tables", 200, 560, "Классика: на поле должны остаться только Хранители.",
     "Используй местность: вода, обрывы, лес, скалы.", "Учитывают высоту, держат дистанцию, расходят удары."),
]


def hero_panel(h):
    def panel(layer, t):
        d = ImageDraw.Draw(layer)
        y0 = PANEL_Y
        p = portrait(h["id"])
        s = ease_back((t - 0.2) / 0.5)
        if p and s > 0:
            ic = icon(p, 150).resize((max(1, int(150 * s)),) * 2)
            layer.alpha_composite(ic, (60 + (150 - ic.width) // 2, y0 + (150 - ic.height) // 2))
        text_block(layer, 236, y0 + 4, 640, h["name"], t, 0.2, F[(58, "title")], GOLD)
        text_block(layer, 236, y0 + 78, 640, h["class"] + " · " + h["role"], t, 0.35, F[(26, "body")], DIM)
        if not h["playable"]:
            q = ease_out((t - 0.5) / 0.4)
            if q > 0:
                label = "ОТКРЫВАЕТСЯ: " + h["unlock"].upper()
                tw = d.textlength(label, font=F[(24, "bold")])
                d.rounded_rectangle((236, y0 + 118, 236 + tw + 30, y0 + 156), 18, fill=RED + (int(255 * q),))
                d.text((251, y0 + 122), label, font=F[(24, "bold")], fill=(255, 255, 255, int(255 * q)))
        bar(d, 60, y0 + 170, 420, "Здоровье", h["hp"], 50, RED, t, 0.4)
        bar(d, 60, y0 + 210, 420, "Движение", h["mp"], 6, BLUE, t, 0.55)
        bar(d, 60, y0 + 250, 420, "Действия", h["ap"], 4, GOLD, t, 0.7)
        for k, sid in enumerate(h["spells"]):
            sp = SPELLS.get(sid, {"name": sid, "numbers": ""})
            card(layer, 60, y0 + 300 + k * 106, 860, 100, sp["name"], sp["numbers"], spell_icon(sid), t, 1.0 + k * 4.0)
    return panel


def ability_panel(sid):
    sp = SPELLS[sid]

    def panel(layer, t):
        d = ImageDraw.Draw(layer)
        y0 = PANEL_Y
        s = ease_back((t - 0.2) / 0.5)
        if s > 0:
            ic = icon(spell_icon(sid), 190).resize((max(1, int(190 * s)),) * 2)
            layer.alpha_composite(ic, (60 + (190 - ic.width) // 2, y0 + (190 - ic.height) // 2))
        text_block(layer, 280, y0 + 10, 600, sp["name"], t, 0.3, F[(58, "title")], GOLD)
        q = ease_out((t - 0.6) / 0.4)
        if q > 0:
            tw = d.textlength(sp["numbers"], font=F[(26, "body")])
            d.rounded_rectangle((280, y0 + 92, 280 + min(tw, 600) + 30, y0 + 134), 20, fill=(255, 255, 255, int(30 * q)))
            d.text((295, y0 + 96), sp["numbers"], font=F[(26, "body")], fill=CREAM + (int(255 * q),))
        y = text_block(layer, 60, y0 + 230, 860, sp["text"], t, 0.9, F[(34, "bold")], CREAM)
        tip = SPELL_TIPS.get(sid)
        if tip:
            card(layer, 60, y0 + 380, 860, 150, "Совет", tip, "Chance", t, 2.2)
        if 4.0 <= t < 8.0:
            d.rounded_rectangle((W - 260, VID_Y + 20, W - 50, VID_Y + 66), 20, fill=(16, 18, 26, 200))
            d.text((W - 240, VID_Y + 24), "ПОВТОР ×0.5", font=F[(24, "bold")], fill=GOLD)
    return panel


def enemy_panel(e):
    def panel(layer, t):
        y0 = PANEL_Y
        d = ImageDraw.Draw(layer)
        p = portrait(e["id"])
        s = ease_back((t - 0.2) / 0.5)
        if p and s > 0:
            ic = icon(p, 150).resize((max(1, int(150 * s)),) * 2)
            layer.alpha_composite(ic, (60 + (150 - ic.width) // 2, y0 + (150 - ic.height) // 2))
        text_block(layer, 236, y0 + 4, 640, e["name"], t, 0.2, F[(58, "title")], RED)
        text_block(layer, 236, y0 + 78, 640, "враг · " + ", ".join(SPELLS.get(s_, {"name": s_})["name"] for s_ in e["spells"]), t, 0.35, F[(26, "body")], DIM)
        bar(d, 60, y0 + 176, 420, "Здоровье", e["hp"], 40, RED, t, 0.5)
        bar(d, 60, y0 + 220, 420, "Движение", e["mp"], 10, BLUE, t, 0.65)
        text_block(layer, 60, y0 + 280, 860, e["text"], t, 1.2, F[(30, "bold")], CREAM)
        card(layer, 60, y0 + 420, 860, 150, "Как победить", ENEMY_TIPS[e["id"]], "Fight", t, 3.0, RED)
    return panel


def event_panel(ev):
    letters = "ABC"

    def panel(layer, t):
        y0 = PANEL_Y
        d = ImageDraw.Draw(layer)
        reveal = t >= 8.0
        text_block(layer, 60, y0, 860, ev["text"], t, 0.3, F[(28, "body")], CREAM)
        n = len(ev["options"])
        h = 132 if n == 3 else 170
        for k, o in enumerate(ev["options"]):
            y = y0 + 150 + k * (h + 10)
            effect = {"None": "Leave", "Gold": "Gold", "Artifact": "Relic", "Armor": "Armor", "Boots": "Boots", "Glory": "Glory",
                      "Fight": "Fight", "FightMercenaries": "Fight"}[o["effect"]]
            if not reveal:
                extra = []
                if o["cost"]:
                    extra.append("цена %d зол." % o["cost"])
                if o["chance"] < 100:
                    extra.append("шанс %d%%" % o["chance"])
                if o["effect"].startswith("Fight"):
                    extra.append("будет бой")
                card(layer, 60, y, 860, h, o["label"].split(" (")[0], " · ".join(extra) or "без риска", "Chance" if o["chance"] < 100 else effect,
                     t, 1.0 + k * 0.4, number=letters[k])
            else:
                text = o["outcome"] or "Бой!"
                if o["chance"] < 100 and o["fail_outcome"]:
                    text = "%d%%: %s / иначе: %s" % (o["chance"], o["outcome"], o["fail_outcome"])
                card(layer, 60, y, 860, h, o["label"].split(" (")[0], text, effect, t, 8.0 + k * 0.6, GREEN, number=letters[k])
        countdown(layer, t, 5.0, 3)
        if 1.5 <= t < 8.0:
            q = ease_out((t - 1.5) / 0.4)
            txt = "Пиши в комментариях: A, B или C?"
            tw = d.textlength(txt, font=F[(30, "bold")])
            d.rounded_rectangle((60, VID_Y + VID_H - 70, 60 + tw + 40, VID_Y + VID_H - 20), 24, fill=GOLD + (int(240 * q),))
            d.text((80, VID_Y + VID_H - 66), txt, font=F[(30, "bold")], fill=INK + (int(255 * q),))
    return panel


RELIC_ICONS = {"StormFeather": "Icon_WindGust", "AncestorStone": "Icon_RaiseRock", "WorldTreeRoot": "Icon_Roots", "RangerBoots": "Move",
               "ScaleMail": "Armor", "ShepherdBell": "Bell", "SaltyBiscuit": "Icon_SlimeBall", "OakHeart": "Icon_Sprout", "WarDrum": "Drum",
               "VeteranCloak": "Cloak", "HawkFeather": "Feather", "DruidAcorn": "Acorn", "CampEmber": "Ember", "LuckyCoin": "Coin",
               "GloryBanner": "Banner", "SwiftCharm": "Charm"}


def relic_icon(relic_id):
    return RELIC_ICONS.get(relic_id, "Relic")


def relic_panel(relics):
    def panel(layer, t):
        for k, r in enumerate(relics):
            card(layer, 60, PANEL_Y + k * 140, 860, 128, r["name"], r["text"], relic_icon(r["id"]), t, 0.6 + k * 1.6, number="#%d" % (k + 1))
    return panel


def objective_panel(o):
    def panel(layer, t):
        card(layer, 60, PANEL_Y, 860, 160, "Правило", o[4], "Glory", t, 0.4)
        card(layer, 60, PANEL_Y + 180, 860, 160, "Как победить", o[5], "Fight", t, 2.4, GREEN)
        card(layer, 60, PANEL_Y + 360, 860, 160, "Что делают враги", o[6], "Unknown", t, 4.4, RED)
    return panel


def tip_panel(number, tip):
    title, text, bullets = tip[0], tip[1], tip[2]

    def panel(layer, t):
        d = ImageDraw.Draw(layer)
        p = ease_back((t - 0.2) / 0.5)
        if p > 0:
            d.text((60, PANEL_Y - 10), "#%d" % number, font=F[(130, "num")], fill=GOLD + (int(255 * min(1, p)),))
        text_block(layer, 300, PANEL_Y + 20, 620, title, t, 0.4, F[(46, "title")], CREAM)
        y = text_block(layer, 60, PANEL_Y + 190, 860, text, t, 1.0, F[(30, "bold")], CREAM)
        for k, b in enumerate(bullets):
            card(layer, 60, PANEL_Y + 330 + k * 112, 860, 100, b, "", "Action" if k == 0 else "Leave", t, 3.0 + k * 1.2)
    return panel


def numbers_panel(layer, t):
    items = [(6, "героев"), (24, "способности"), (20, "событий"), (16, "реликвий"), (5, "заданий боя"), (9, "видов врагов")]
    for k, (v, label) in enumerate(items):
        x = 60 + (k % 2) * 440
        y = PANEL_Y + (k // 2) * 200 - 20
        big_number(layer, x, y, v, label, t, 0.4 + k * 0.8, [GOLD, BLUE, GREEN, GOLD, RED, CREAM][k])


def campaign_panel(layer, t):
    nodes = [("Бой", "враги и награда", "Fight", RED), ("Элита", "сильнее, богаче", "Glory", (160, 90, 200)),
             ("Руины", "реликвия на выбор", "Relic", BLUE), ("Событие", "выбор и последствия", "Chance", GREEN),
             ("Привал", "кузница и торговец", "Ember", GOLD), ("Босс", "конец региона", "Fight", (200, 60, 60))]
    for k, (name, text, ic, col) in enumerate(nodes):
        x = 60 + (k % 2) * 440
        y = PANEL_Y + (k // 2) * 176
        card(layer, x, y, 420, 160, name, text, ic, t, 0.5 + k * 0.7, col)


def ai_panel(layer, t):
    d = ImageDraw.Draw(layer)
    text_block(layer, 60, PANEL_Y - 6, 860, "Каждый враг оценивает сотни вариантов удара. К урону добавляется:", t, 0.3, F[(30, "bold")], CREAM)
    rows = [("Толкнуть героя в воду", 6.0, GREEN), ("Добить раненого", 1.0, GREEN), ("Высота над целью", 0.8, GREEN),
            ("Держать дистанцию (стрелки)", 0.9, GREEN), ("Клетка у обрыва", -1.8, RED), ("Удар туда же, куда бьёт другой", -1.0, RED)]
    for k, (label, v, col) in enumerate(rows):
        y = PANEL_Y + 110 + k * 72
        p = ease_out((t - 1.2 - k * 0.6) / 0.6)
        if p <= 0:
            continue
        d.text((60, y), label, font=F[(26, "body")], fill=CREAM + (int(255 * p),))
        w = int(abs(v) / 6.0 * 300 * p)
        x0 = 640
        if v > 0:
            d.rounded_rectangle((x0, y + 4, x0 + max(w, 6), y + 32), 10, fill=col + (255,))
        else:
            d.rounded_rectangle((x0 - max(w, 6), y + 4, x0, y + 32), 10, fill=col + (255,))
        d.text((x0 + (w + 14 if v > 0 else 14), y - 2), ("+" if v > 0 else "") + ("%g" % v), font=F[(40, "num")], fill=CREAM + (int(255 * p),))


def sound_panel(layer, t):
    d = ImageDraw.Draw(layer)
    text_block(layer, 60, PANEL_Y - 6, 860, "В игре нет ни одного чужого звука: удары, шлепки, ветер и музыка синтезированы кодом.", t, 0.3, F[(30, "bold")], CREAM)
    import math as m
    for k in range(5):
        y = PANEL_Y + 170 + k * 80
        label = ["удар", "слизь", "ветер", "лечение", "монета"][k]
        p = ease_out((t - 1.0 - k * 0.5) / 0.5)
        if p <= 0:
            continue
        d.text((60, y + 10), label, font=F[(26, "bold")], fill=GOLD + (int(255 * p),))
        pts = []
        for i in range(140):
            x = 220 + i * 5
            amp = 26 * m.exp(-i / [20, 50, 90, 60, 30][k]) * (1 if k != 2 else m.sin(m.pi * i / 140) * 1.5)
            pts.append((x, y + 28 + amp * m.sin(i * [1.7, 0.6, 2.9, 1.1, 2.2][k] + t * 6)))
        d.line(pts[:int(len(pts) * p)], fill=CREAM + (220,), width=3)


def director_panel(layer, t):
    steps = [("Запуск игры в окне", "отдельный процесс, фиксированный шаг 30 кадров"),
             ("Автобой", "все юниты под ИИ, бой ускорен ×2.5"),
             ("Камера-режиссёр", "держит в кадре ходящего и его цель"),
             ("Склейка", "титры, подписи и музыка — скриптом")]
    for k, (a, b) in enumerate(steps):
        card(layer, 60, PANEL_Y + k * 140, 860, 128, a, b, ["Action", "Fight", "Move", "Glory"][k], t, 0.5 + k * 1.5, number=str(k + 1))


def poll_panel(layer, t):
    d = ImageDraw.Draw(layer)
    letters = "ABCDEF"
    for k, h in enumerate(DATA["heroes"]):
        x = 60 + (k % 3) * 290
        y = PANEL_Y + (k // 3) * 270
        p = ease_back((t - 0.4 - k * 0.35) / 0.5)
        if p <= 0:
            continue
        d.rounded_rectangle((x, y, x + 270, y + 250), 22, fill=(24, 26, 36, 235), outline=GOLD + (255,), width=3)
        pic = portrait(h["id"])
        if pic:
            ic = icon(pic, 140).resize((max(1, int(140 * min(p, 1.1))),) * 2)
            layer.alpha_composite(ic, (x + 135 - ic.width // 2, y + 20))
        else:
            d.text((x + 100, y + 50), "?", font=F[(90, "num")], fill=DIM)
        d.text((x + 16, y + 170), "%s · %s" % (letters[k], h["name"]), font=F[(30, "bold")], fill=CREAM)
        d.text((x + 16, y + 208), h["class"], font=F[(22, "body")], fill=DIM)


def growth_panel(layer, t):
    cards = [("Закалка", "+5 к здоровью", "Hp", RED), ("Лёгкий шаг", "+1 к движению", "Move", BLUE), ("Боевой опыт", "+1 к действиям", "Action", GOLD)]
    text_block(layer, 60, PANEL_Y - 6, 860, "После каждой победы один герой растёт — до 5 уровня. Ты выбираешь, как.", t, 0.3, F[(30, "bold")], CREAM)
    for k, (a, b, ic, col) in enumerate(cards):
        card(layer, 60, PANEL_Y + 130 + k * 140, 860, 128, a, b, ic, t, 1.0 + k * 0.8, col)


def highlight_panel(level, objective):
    def panel(layer, t):
        card(layer, 60, PANEL_Y, 860, 130, level, objective, "Fight", t, 0.3)
        big_number(layer, 60, PANEL_Y + 170, 7, "раз быстрее, чем в игре", t, 0.8, GOLD)
        text_block(layer, 520, PANEL_Y + 200, 400, "Весь бой играет ИИ — и за врагов, и за героев.", t, 1.6, F[(30, "bold")], CREAM)
    return panel


def catalog():
    shorts = []

    def add(slug, series, title, seconds, footage, panel, caption, tags="", accent=GOLD, pops=None):
        shorts.append({"slug": slug, "series": series, "title": title, "seconds": seconds, "footage": footage, "panel": panel,
                       "caption": caption, "tags": TAGS + (" " + tags if tags else ""), "accent": accent,
                       "pops": pops or [(0.25, "Card")]})

    # Heroes.
    for h in DATA["heroes"]:
        add("hero_" + h["id"].lower(), "ГЕРОЙ", "%s — %s" % (h["name"], h["class"].lower()), 13.0,
            Footage([("hero_" + h["id"], 0, 390, 1)], 1.0, (0.0, 0.1, 0.6, 1.0)), hero_panel(h),
            "%s. %s Какой герой нужен твоему отряду?" % (h["name"], h["text"]), "#персонаж #фэнтези",
            pops=[(0.2, "LevelUp"), (1.0, "Card"), (5.0, "Card"), (9.0, "Card")])
    # Abilities of the playable heroes, with a slow-motion replay.
    for h in DATA["heroes"][:3]:
        for k, sid in enumerate(h["spells"]):
            start = 15 + k * 120
            f = Footage([("hero_" + h["id"], start, start + 120, 1)], 1.0, (0.02, 0.12, 0.78, 1.0))
            slow = Footage([("hero_" + h["id"], start + 10, start + 70, 1)])
            f.files += [x for x in slow.files for _ in (0, 1)]
            add("skill_" + sid.lower(), "СПОСОБНОСТЬ", "%s: %s" % (h["name"], SPELLS[sid]["name"]), 8.0, f, ability_panel(sid),
                "%s — %s. %s" % (SPELLS[sid]["name"], SPELLS[sid]["text"], SPELL_TIPS.get(sid, "")), "#способности",
                pops=[(0.2, "Hit"), (2.2, "Card")])
    # Enemies.
    enemy_clips = {"Slime": ("b_slimes", 200, 560), "Chicken": ("b_chickens", 200, 560), "Bat": ("b_bats", 200, 560),
                   "Bandit": ("b_mercs", 100, 460), "BanditArcher": ("b_mercs", 460, 820), "Brute": ("obj_leader", 300, 660)}
    for e in DATA["enemies"]:
        clip, a, b = enemy_clips[e["id"]]
        add("enemy_" + e["id"].lower(), "БЕСТИАРИЙ", e["name"], 12.0, Footage([(clip, a, b, 1)]), enemy_panel(e),
            "%s: %s %s" % (e["name"], e["text"], ENEMY_TIPS[e["id"]]), "#бестиарий", RED, pops=[(0.2, "Slime" if e["id"] == "Slime" else "Hit"), (3.0, "Card")])
    # Events: "what would you choose?".
    for k in range(20):
        ev = DATA["events"][(1 + 13 * k) % 20]
        add("event_%02d" % ((1 + 13 * k) % 20), "ЧТО ВЫБЕРЕШЬ?", ev["title"], 14.0,
            StillFootage(r"C:/ProjectsUE/_logs/v_ev_%d.png" % k, (230, 150, 1067, 640), 14), event_panel(ev),
            "%s. %s Что выберешь ты? Ответ — в конце ролика." % (ev["title"], ev["text"]), "#выбор #рпг", GREEN,
            pops=[(1.0, "Card"), (1.4, "Card"), (1.8, "Card"), (5.0, "Hit"), (6.0, "Hit"), (7.0, "Hit"), (8.0, "Coin")])
    # Relics: four tops of four.
    relics = DATA["relics"]
    tops = [("4 реликвии для смелых", relics[0:4]), ("4 реликвии для крепких", relics[4:8]),
            ("4 реликвии для каждого героя", relics[8:12]), ("4 реликвии для богатых", relics[12:16])]
    for k, (title, group) in enumerate(tops):
        add("relics_%d" % (k + 1), "РЕЛИКВИИ", title, 8.0, Footage([("cards", 700, 800, 1), ("battle", 300 + k * 200, 400 + k * 200, 1)]),
            relic_panel(group), "%s: %s. Какую возьмёшь первой?" % (title, ", ".join(r["name"] for r in group)), "#реликвии #роглайт",
            pops=[(0.6 + i * 1.6, "Coin") for i in range(4)])
    # Objectives.
    for o in OBJECTIVES:
        add("goal_" + o[0], "ЗАДАНИЕ БОЯ", o[1], 9.0, Footage([(o[2], o[3], o[4], 1)]), objective_panel(o),
            "%s. %s %s" % (o[1], o[4], o[5]), "#тактика", BLUE, pops=[(0.4, "Card"), (2.4, "Card"), (4.4, "Card")])
    # Tactics tips.
    for k, tip in enumerate(TIPS, 1):
        add("tip_%02d" % k, "СОВЕТ", tip[0], 9.0, Footage([(tip[3], tip[4], tip[5], 1)]), tip_panel(k, tip),
            "Совет #%d: %s. %s" % (k, tip[0].lower(), tip[1]), "#советы #гайд", GREEN, pops=[(0.2, "Coin"), (3.0, "Card"), (4.2, "Card")])
    # Battle highlights.
    highlights = [("b_chickens", "Охота на кур", "куры зовут подмогу"), ("b_bats", "Летать — нечестно", "летучие мыши над водой"),
                  ("b_tables", "Всё наоборот", "врагов меньше, но они умнее"), ("b_chess", "Шахматы на четверых", "четыре отряда на одной доске"),
                  ("b_protect", "Защити курицу", "Пеструшка не должна погибнуть"), ("b_mercs", "Охота на вожака", "наёмники кур и громила"),
                  ("b_slimes", "Слишком скользко", "слизни делятся надвое"), ("b_orbit", "Слишком много врагов", "врагов втрое больше")]
    for clip, level, objective in highlights:
        add("battle_" + clip[2:], "БОЙ ЗА 15 СЕКУНД", level, 15.0, Footage([(clip, 0, 1350, 3)]), highlight_panel(level, objective),
            "%s — %s. Весь бой за 15 секунд, играет ИИ." % (level, objective), "#геймплей", RED, pops=[(0.3, "Card")])
    # Infographics and devlog.
    add("numbers", "ИГРА В ЦИФРАХ", "Хранители земель в цифрах", 8.0, Footage([("battle2", 100, 400, 1)]), numbers_panel,
        "6 героев, 24 способности, 20 событий, 16 реликвий, 5 заданий боя и 9 видов врагов. Что добавить следующим?", "#инфографика",
        pops=[(0.4 + k * 0.8, "Coin") for k in range(6)])
    add("campaign", "КАМПАНИЯ", "Как устроен поход", 9.0, Footage([("campaign", 20, 300, 1)]), campaign_panel,
        "Кампания — карта узлов: бои, элита, руины, события, привал и босс. Каждый поход — новая карта.", "#роглайт",
        pops=[(0.5 + k * 0.7, "Card") for k in range(6)])
    add("growth", "РОСТ ГЕРОЕВ", "Герой растёт после каждой победы", 8.0, Footage([("cards", 600, 720, 1)]), growth_panel,
        "После победы один герой получает уровень: здоровье, движение или очко действия. Что выберешь?", "#рпг",
        pops=[(0.3, "LevelUp"), (1.0, "Card"), (1.8, "Card"), (2.6, "Card")])
    add("devlog_ai", "КАК ЭТО СДЕЛАНО", "Как думают враги", 10.0, Footage([("b_mercs", 300, 600, 1)]), ai_panel,
        "ИИ врагов: к урону добавляются тактические бонусы — толкнуть в воду, добить раненого, занять высоту.", "#девлог #ии",
        BLUE, pops=[(1.2 + k * 0.6, "Card") for k in range(6)])
    add("devlog_sound", "КАК ЭТО СДЕЛАНО", "Звуки — из кода", 9.0, Footage([("battle", 1160, 1340, 1)]), sound_panel,
        "Все звуки и музыка игры синтезированы кодом: никаких чужих сэмплов.", "#девлог #звук", BLUE,
        pops=[(1.0, "Hit"), (1.5, "Slime"), (2.0, "Gust"), (2.5, "Heal"), (3.0, "Coin")])
    add("devlog_director", "КАК ЭТО СДЕЛАНО", "Игра сама снимает трейлер", 9.0, Footage([("obj_reach", 300, 570, 1)]), director_panel,
        "Ролики снимает сама игра: автобой, камера-режиссёр и склейка скриптом.", "#девлог", BLUE,
        pops=[(0.5 + k * 1.5, "Card") for k in range(4)])
    add("poll_main", "ОПРОС", "Кто твой мейн?", 10.0, Footage([("squad6", 0, 360, 1)]), poll_panel,
        "Шесть героев: Страж, Следопыт, Друид, Маг, Разбойник, Варвар. Кто твой мейн? Пиши букву в комментарии!", "#опрос",
        pops=[(0.4 + k * 0.35, "Card") for k in range(6)])
    return shorts
