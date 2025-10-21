## Virtual machines HW 1. Cache parameters

Нужно запустить main. Код может исполняться несколько десятков секунд. Если это слишком долго, можно уменьшить `REPEATS` 
в [main.cpp](main.cpp). По умолчанию код запускается на ядре процессора с id 0.

Пример вывода:
```
Entity has line size 64
Entity has line size 4096
Entity with assoc 6 has entity stride 65536 Bytes and size 393216 Bytes
Entity with assoc 12 has entity stride 4096 Bytes and size 49152 Bytes
```
или
```
Entity has line size 64
Entity with assoc 8 has entity stride 4096 Bytes and size 32768 Bytes
```

Если одна из характеристик не была напечатана, программа не смогла её определить. 
Если было напечатано несколько одинаковых характеристик, было зафиксировано несколько сущностей.

Иногда по неизвестным причинам в результатах измерений встречаются выбросы, тогда результат работы программы может быть неверным.
Бороться с этим помогает запуск программы несколько раз и фиксирование тактовой частоты ядер процессора.

---

To check CPU frequency:
`watch -n 1 "grep \"^[c]pu MHz\" /proc/cpuinfo"`

To set CPU frequency:
`sudo cpupower frequency-set --min 4GHz --max 4GHz`

To determine real cache parameters:
* `cat /sys/devices/system/cpu/cpu12/cache/index0/size`
* `cat /sys/devices/system/cpu/cpu12/cache/index0/coherency_line_size`
* `cat /sys/devices/system/cpu/cpu12/cache/index0/ways_of_associativity`
