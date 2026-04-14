# Шрифти для дисплея

Ця папка має містити файли шрифтів у форматі `.vlw`:

- `Font16.vlw` - малий шрифт (16pt)
- `Font24.vlw` - середній шрифт (24pt)
- `Font32.vlw` - великий шрифт (32pt)
- `Font48.vlw` - дуже великий шрифт (48pt)

## Як створити .vlw файли

1. Встановіть [Processing](https://processing.org/)
2. Відкрийте скетч: `TFT_eSPI/tools/Create_Smooth_Font/Create_font/Create_font.pde`
3. Оберіть TTF шрифт (наприклад, `FreeSans.ttf` з папки `TFT_eSPI/Fonts/GFXFF`)
4. Вкажіть розмір (16, 24, 32, 48)
5. Натисніть "Create" - файл `.vlw` буде збережено
6. Скопіюйте файли в цю папку `data/fonts/`

## Альтернатива

Можна завантажити готові шрифти з репозиторію TFT_eSPI:
https://github.com/Bodmer/TFT_eSPI/tree/master/Fonts
