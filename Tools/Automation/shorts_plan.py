"""The posting calendar for the shorts: a month, 2-3 a day, series interleaved.

Usage: python shorts_plan.py [first day YYYY-MM-DD]  ->  C:/ProjectsUE/_shorts/КОНТЕНТ_ПЛАН.md and plan.csv
"""
import csv
import datetime
import json
import os
import sys

OUT = r"C:/ProjectsUE/_shorts"


def main(first):
    shorts = json.load(open(os.path.join(OUT, "plan.json"), encoding="utf-8"))
    for k, s in enumerate(shorts, 1):
        s["file"] = "%02d_%s.mp4" % (k, s["slug"])
    by_series = {}
    for s in shorts:
        by_series.setdefault(s["series"], []).append(s)
    # The first day introduces the game; then the series take turns, the strongest formats more often.
    order = ["ИГРА В ЦИФРАХ", "ГЕРОЙ", "ЧТО ВЫБЕРЕШЬ?", "СОВЕТ", "БОЙ ЗА 15 СЕКУНД", "СПОСОБНОСТЬ", "ЧТО ВЫБЕРЕШЬ?",
             "БЕСТИАРИЙ", "ЗАДАНИЕ БОЯ", "СОВЕТ", "ЧТО ВЫБЕРЕШЬ?", "РЕЛИКВИИ", "КАК ЭТО СДЕЛАНО", "КАМПАНИЯ", "РОСТ ГЕРОЕВ", "ОПРОС"]
    queue = []
    while any(by_series.values()):
        for series in order:
            if by_series.get(series):
                queue.append(by_series[series].pop(0))
        for series, rest in by_series.items():
            if rest and series not in order:
                queue.append(rest.pop(0))
    day = datetime.date.fromisoformat(first)
    rows = []
    i = 0
    d = 0
    while i < len(queue):
        slots = ["12:00", "16:00", "20:00"] if d < 17 else ["13:00", "19:00"]
        for slot in slots:
            if i < len(queue):
                s = queue[i]
                date = (day + datetime.timedelta(days=d)).isoformat()
                # Files go into a folder per day, named by the posting time.
                folder = "День %02d (%s)" % (d + 1, date)
                target = os.path.join(folder, "%s %s.mp4" % (slot.replace(":", "-"), s["slug"]))
                source = os.path.join(OUT, s["file"])
                os.makedirs(os.path.join(OUT, folder), exist_ok=True)
                if os.path.exists(source):
                    os.replace(source, os.path.join(OUT, target))
                s["file"] = target.replace("\\", "/")
                rows.append({"день": d + 1, "дата": (day + datetime.timedelta(days=d)).isoformat(), "время": slot, "файл": s["file"],
                             "серия": s["series"], "заголовок": s["title"], "описание": s["caption"], "теги": s["tags"]})
                i += 1
        d += 1
    with open(os.path.join(OUT, "plan.csv"), "w", encoding="utf-8-sig", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()), delimiter=";")
        w.writeheader()
        w.writerows(rows)
    lines = ["# Контент-план: «Хранители земель» в TikTok, YouTube Shorts и Reels", "",
             "%d роликов, %d дней. Дни 1–17 — по три публикации (12:00, 16:00, 20:00), дальше — по две (13:00, 19:00)." % (len(rows), d),
             "Формат: 1080×1920, 30 к/с, H.264 + AAC, 8–15 секунд. Файлы — в этой папке.", "",
             "Как публиковать:",
             "- описание и теги — из таблицы ниже (или `plan.csv`, открывается в Excel);",
             "- в «Что выберешь?» закрепи комментарий «Пишите A, B или C — ответ в конце ролика»;",
             "- в «Опросе» — «Кто твой мейн? Буква в комментарии»;",
             "- отвечай на первые комментарии в первый час: это поднимает охваты.", ""]
    current = None
    for r in rows:
        if r["день"] != current:
            current = r["день"]
            lines += ["", "## День %d — %s" % (r["день"], r["дата"]), ""]
        lines.append("- **%s** · `%s` · %s — «%s»" % (r["время"], r["файл"], r["серия"].lower(), r["заголовок"]))
        lines.append("  - %s" % r["описание"])
        lines.append("  - %s" % r["теги"])
    open(os.path.join(OUT, "КОНТЕНТ_ПЛАН.md"), "w", encoding="utf-8").write("\n".join(lines) + "\n")
    print(len(rows), "posts over", d, "days")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else (datetime.date.today() + datetime.timedelta(days=1)).isoformat())
