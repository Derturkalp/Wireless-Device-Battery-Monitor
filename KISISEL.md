# BatteryHub – kişisel sürüm

Orijinal BatteryHub v1.1.0 üzerine yapılan değişiklikler:

| Ne | Orijinal | Bu sürüm |
|---|---|---|
| Rakamın kırmızıya döndüğü seviye | %15 | **%20** |
| Turuncu "azalıyor" çubuğu (tray ikonunda) | %30 | **%40** |
| Varsayılan "Düşük pil" bildirimi | %15 | **%20** (cihaz menüsünden yine 5–40 arası değiştirilebilir) |
| ATK / VXE cihazlar (mouse, N9 kulaklık) | marka rengi yok | **camgöbeği** |
| MCHOSE cihazlar (klavye) | marka rengi yok | **mor** |
| Xbox kolu | yeşil | yeşil (değişmedi) |

Marka renkleri menüde *Brand colors* (Marka renkleri) açıkken görünür.

Renkleri değiştirmek için: `src/brands.cpp` içindeki `Accent::Atk` ve `Accent::Mchose` satırlarındaki
`{ R, G, B }` değerleri (ilki koyu görev çubuğu, ikincisi açık görev çubuğu için).
Eşikler için: `src/main.cpp` → `LOW_BATTERY`, `src/alert_rules.h` → `kDefaultThreshold`.

## .exe nasıl alınır (bilgisayarda derleyici kurmadan)

1. GitHub'da orijinal depoyu **Fork** et.
2. Fork'unun **Actions** sekmesinde iş akışlarını etkinleştir.
3. Bu klasördeki dosyaları fork'unun `main` dalına yükle (ya da `kisisel.patch` dosyasını `git am kisisel.patch` ile uygula ve push et).
4. Actions içindeki "build" çalışması bitince **Artifacts → BatteryHub** zip'ini indir; içinde `BatteryHub.exe` ve kurulum dosyası var.

Ya da Windows'ta MSYS2 kurup (`pacman -S mingw-w64-x86_64-gcc make`) klasörde `build.bat` çalıştır.

## Cihazların için notlar

- **ATK N9 kulaklık:** ATK masaüstü uygulaması açıkken pil okunamaz, onu kapat.
- **ATK mouse:** `BatteryHub.exe --list` ile görünüyor mu kontrol et; görünmüyorsa `--dump` çıktısını sakla.
- **MCHOSE klavye:** modeline göre 2.4 GHz alıcıda çalışır; Bluetooth/kablo modları her modelde desteklenmiyor.
- **Xbox kolu:** Xbox kablosuz adaptör, USB veya Bluetooth ile doğrudan çalışır.
- Görünmesini istemediğin bir cihaz olursa menüden üstüne tıklayıp gizleyebilirsin.
