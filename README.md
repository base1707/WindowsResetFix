<p align="center"><img src="https://sun9-39.userapi.com/impg/nSoOziIrGuSVF8VLrTADH1zBWdsby9ZRARBhhg/RGryRm88ZaU.jpg?size=650x300&quality=96&sign=4ded18d96654e18f626ada340ee9a32a"/></p>

## WindowsResetFix
Служба Windows, затыкающая дыру с подменой стандартных утилит на экране входа в ОС:
- __Utilman.exe__
- __find.exe__
- __osc.exe__
- __sethc.exe__

В момент своего первого запуска производит кеширование уязвимых файлов, а при каждой последующей загрузке системы сверяет сохранённые значения с актуальными. В случае расхождений система будет автоматически выключена.

## Зависимости
Для сборки проекта потребуются:
- __Visual Studio 2022__ (и выше)
- __С++17__ (и выше)
- Поддержка __WindowsAPI__ и __Wtsapi32__

## Установка
Инсталляция службы производится через консоль (__cmd.exe__), запущенную с правами администратора:
```console
WindowsResetFix.exe install
```
Служба автоматически запустится при следующем входе в систему.

## Деинсталляция
```console
WindowsResetFix.exe uninstall
```