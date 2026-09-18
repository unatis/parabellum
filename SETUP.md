# Parabellum — развёртывание с нуля

Что нужно, чтобы на новой машине (или в новой сессии ИИ) склонировать проект и продолжить
с того места, где остановились.

Смежные файлы: [CLAUDE.md](CLAUDE.md) — принятые решения и почему;
[HANDOFF.md](HANDOFF.md) — где именно мы остановились;
[Docs/METHOD.md](Docs/METHOD.md) — способ построения моделей оружия;
[Docs/ARCHITECTURE.md](Docs/ARCHITECTURE.md) — карта кода, данных и проверок;
[BACKLOG.md](BACKLOG.md) — что сделано и что дальше.

---

## 1. Что поставить

| Что | Версия | Зачем |
|---|---|---|
| Visual Studio | **2026 (18.10)** | компилятор. Воркоады: *Desktop development with C++*, *Game development with C++*, *.NET desktop development*. Компоненты: *Visual Studio Tools for Unreal Engine*, *HLSL Tools*, **MSVC v143 v14.44-17.14** (без него UE 5.8 не собирается) |
| Unreal Engine | **5.8.2** | через Epic Games Launcher, в `C:\Program Files\Epic Games\UE_5.8` |
| Blender | **5.2.1** | в `C:\Program Files\Blender Foundation\Blender 5.2`. Все модели и часть текстур делаются здесь, headless |
| Git | любой | репозиторий `https://github.com/unatis/parabellum.git` |
| Python | 3.x, системный | только для мелких вспомогательных скриптов; внутри Blender и UE свои интерпретаторы |

Плагины UE включены в `Parabellum.uproject` и подтянутся сами: *EnhancedInput*,
*PythonScriptPlugin*, *EditorScriptingUtilities*.

**Если пути другие** — поправить их в трёх местах: `Editor.bat`, `Play.bat`, `Tools/*.bat`
(движок) и `Tools/blender/*.bat` (Blender). Пути захардкожены намеренно: один раз на машину.

---

## 2. Клонировать и собрать

```bash
git clone https://github.com/unatis/parabellum.git C:\AI_Claude\Parabellum
```

`Content/` лежит в репозитории целиком (247 файлов, ~300 МБ: меши, текстуры, звуки, уровень,
ассеты ввода). После клона проект запускается сразу — генерировать ассеты заново не нужно.

Сборка:

```bash
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" ParabellumEditor Win64 Development -Project="C:\AI_Claude\Parabellum\Parabellum.uproject" -WaitMutex -FromMsBuild
```

**Сборка падает с «Unable to build while Live Coding is active», если открыт редактор или
игра.** Закрыть и собрать заново — это самая частая осечка.

Запуск: `Play.bat` (игра без редактора) или `Editor.bat` (редактор).

---

## 3. Чего нет в репозитории

`Import/` целиком в `.gitignore`: это исходники, из которых получены ассеты, — FBX, архивы,
звуки, фотографии. Полтора гигабайта, лицензии чужие. Проект без них работает; они нужны,
только если пересобирать ассеты.

| Папка | Что там | Как восстановить |
|---|---|---|
| `Import/Glock17`, `Glock17CAD` | сторонние модели Glock | Fab / Sketchfab, ссылки в истории коммитов |
| `Import/Mixamo` | анимации персонажа (1.1 ГБ) | mixamo.com, персонаж Ch15 |
| `Import/Sounds` | выстрелы, удары | Freesound 34982 (gezortenplotz) и звук пользователя; нарезка `Tools/blender/cut_gunshots.py` |
| `Import/Surfaces/megascans` | сканы поверхностей | Fab, **Standard License распространять запрещает**; `Tools/megascans_prepare.py` разложит архивы заново |
| `Import/Hands` | фотографии рук пользователя | снять заново; `Tools/blender/make_hand_texture.py` сделает из них текстуру |
| `Import/Reference/*` | фотографии оружия и снятые с них контуры | **контуры (`profile.json`) в репозитории есть** — см. ниже |
| `Import/*Parts`, `Import/Ammo`, `Import/GelDummy` | FBX, которые делают наши же скрипты | пересобрать: `Tools/blender/make_*.bat` |

**Контуры с фотографий (`Reference/Contours/*.json`) лежат в репозитории.** Это результат
обмера, а не исходник: несколько килобайт точек, восстановить которые без той же фотографии
и той же настройки нельзя. Модель 1911 строится именно из них.

### Пересборка ассетов, если понадобится

Порядок важен: Blender делает FBX/PNG в `Import/`, скрипты UE затаскивают их в `Content/`.

```
Tools\blender\make_glock17.bat      →  Import\Glock17Parts\*.fbx + .json
Tools\blender\make_colt1911.bat     →  Import\Colt1911Parts\*.fbx + .json
Tools\blender\make_cartridge.bat    →  Import\Ammo\*.fbx
Tools\blender\make_gel_dummy.bat    →  Import\GelDummy\*.fbx
Tools\blender\make_surfaces.bat     →  Import\Surfaces\*.png
Tools\blender\make_hand_texture.bat →  Import\Hands\Hand_{D,N,R}.png

Tools\import_weapon_parts.bat   →  Content\Weapons\... + Content\Data\WeaponParts.csv
Tools\import_ammo.bat           →  Content\Weapons\Ammo\...
Tools\import_surfaces.bat       →  Content\Surfaces\...
Tools\import_hand_texture.bat   →  Content\Characters\...
Tools\import_gel_dummy.bat      →  Content\Targets\...
Tools\import_sounds.bat         →  Content\Audio\...
Tools\make_input_assets.bat     →  Content\Input\  (раскладка клавиш)
Tools\make_range_assets.bat     →  Content\Range\  (материалы мишеней)
Tools\make_weapon_assets.bat    →  Content\Weapons\FX\ (трассер, вспышка, декаль)
Tools\make_vision_material.bat  →  Content\Vision\M_FovealComposite
Tools\make_greybox_level.bat    →  Content\Maps\Greybox  (уровень целиком)
```

`.uasset` — двоичный формат, руками и агентом не правится. Всё, что видно в редакторе,
получено этими скриптами; уровень в редакторе не трогать, иначе следующий запуск
`make_greybox_level.bat` затрёт правки.

---

## 4. Проверить, что всё живо

Эти три проверки покрывают почти всё и не требуют открывать редактор.

**Картинка в игре** — `Tools/shot.ps1` запускает игру headless и пишет PNG в
`Saved/Screenshots`:

```powershell
powershell -File Tools\shot.ps1 -Name check -After 12
powershell -File Tools\shot.ps1 -Name aim   -Aim 1 -After 12
```

**Расчётная часть** — консольные команды в игре или через `-ExecCmds`:

```
pbl.Ballistics.Table 9x19_124_FMJ     таблица траектории против опубликованной
pbl.Ballistics.GelCheck               глубина в геле против Reference_Gel.csv
pbl.Ballistics.Cycle Glock17          темп стрельбы из физики отката
```

**Модель оружия** — Blender сам сверяет габариты с таблицей ТТХ и печатает
`dimension check PASSED/FAILED`, а `fit_check.py` — силуэт с фотографией:

```bash
"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b -P Tools/blender/fit_check.py -- Tools/blender/make_colt1911.py Reference/Contours/Colt1911_photo2.json 210
```

Подробнее — [Docs/ARCHITECTURE.md](Docs/ARCHITECTURE.md), раздел «Как проверяется».

---

## 5. Перенос сессии в другой ИИ

Дать прочитать по порядку: `CLAUDE.md` → `HANDOFF.md` → `Docs/METHOD.md` →
`Docs/ARCHITECTURE.md` → `BACKLOG.md`. Этого достаточно, чтобы продолжить: в них записаны
не только решения, но и причины — почему отвергнуто то, что выглядит очевидным.

Главное правило работы, которое стоит передать словами: **ничего не принимается на глаз.**
У каждой модели есть сверка с источником, у каждой формулы — сверка с опубликованной
таблицей. Если проверки нет — её надо сделать раньше, чем результат.
