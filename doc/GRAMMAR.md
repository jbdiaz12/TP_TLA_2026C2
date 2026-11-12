# Gramática de Stujfy

Definición formal de `G = ⟨Σ, N, Π, S⟩`. El analizador léxico
(`FlexPatterns.l`) define `Σ`; el sintáctico (`BisonGrammar.y`) define `N`, `Π` y `S`.
La sección 6 resume la semántica: qué atributos existen, qué valores admiten y
cómo los usa el planificador.

---

## 1. Σ — Alfabeto

### 1.1 Palabras reservadas

```
after   and     availability  constraints  default  for     goal  in
include method  not           or           otherwise         pause plan
repeat  rules   scale         session      topic    when
```

`include` no llega nunca al analizador sintáctico: lo resuelve el léxico (ver §1.5).


### 1.2 Literales

| Terminal | Expresión regular | Ejemplos | Valor semántico |
| :------- | :---------------- | :------- | :-------------- |
| `INTEGER` | `[0-9]+` | `4`, `120` | entero |
| `DURATION` | `([0-9]+[wdhm])+` | `30m`, `2h30m`, `21d` | entero: total de minutos |
| `TIME` | `[0-9]{1,2}:[0-9]{2}` | `18:00` | entero: minutos desde la medianoche |
| `DATE` | `[0-9]{4}-[0-9]{2}-[0-9]{2}` | `2026-12-05` | entero: `aaaa*10000 + mm*100 + dd` |
| `STRING` | `"` sin comillas ni saltos de línea `"` | `"Teoría"` | cadena, sin las comillas |
| `IDENTIFIER` | `[A-Za-z_][A-Za-z0-9_]*` | `Automatas`, `monday` | cadena |

Factores de `DURATION`: `w` = 10080, `d` = 1440, `h` = 60, `m` = 1; un 
compuesto suma sus componentes.

Los literales numéricos conviven sin ambigüedad por el *longest prefix match* de
Flex: `2026-12-05` se mapea a un único `DATE` y no a `INTEGER SUB INTEGER SUB
INTEGER`.

### 1.3 Operadores y signos de puntuación

| Lexema | Terminal |
| :----- | :------- |
| `{` `}` | `OPEN_BRACE` `CLOSE_BRACE` |
| `(` `)` | `OPEN_PARENTHESIS` `CLOSE_PARENTHESIS` |
| `[` `]` | `OPEN_BRACKET` `CLOSE_BRACKET` |
| `;` `:` `,` | `SEMICOLON` `COLON` `COMMA` |
| `->` `..` | `ARROW` `RANGE` |
| `+` `-` `*` `/` | `ADD` `SUB` `MUL` `DIV` |
| `<` `>` `<=` `>=` | `LESS` `GREATER` `LESS_EQUAL` `GREATER_EQUAL` |
| `==` `!=` | `EQUAL` `NOT_EQUAL` |

El lenguaje no define `=`: la asignación de un atributo usa `:` y la igualdad usa
`==`. El operador `..` delimita rangos horarios y es un símbolo propio para que un
rango no resulte indistinguible de una resta.

### 1.4 Lexemas ignorados

Espacios, tabulaciones y saltos de línea; comentarios de línea (`//`) y de bloque
(`/* … */`, mediante el contexto exclusivo `MULTILINE_COMMENT`). Un comentario de
bloque sin cerrar constituye un error.

### 1.5 Inclusión de archivos

Como `#include` en C, la directiva copia el contenido de otro archivo en el lugar
donde aparece. La resuelve el analizador léxico, apilando un nuevo buffer de
entrada, así que el analizador sintáctico recibe directamente los tokens del
archivo incluido y la gramática no cambia. No lleva `;`.

| Forma | Busca |
| :---- | :---- |
| `include <metodos>` | `metodos.stujfy` en la carpeta de la librería estándar (`src/main/stujfy/lib`, o la que indique `STUJFY_LIBRARY_PATH`) |
| `include "ruta.stujfy"` | El archivo, relativo a la carpeta desde la que se ejecuta el compilador |

Cada archivo se incluye una sola vez, como con `#pragma once`: un segundo
`include` del mismo archivo no tiene efecto, y dos archivos que se incluyen
entre sí no generan un ciclo. Un archivo inexistente es un error léxico.

---

## 2. S — Símbolo inicial

```
S = program
```

---

## 3. N — Símbolos no terminales

```
program            declarationList    declaration
goalDeclaration    topicDeclaration   memberList       member       attribute
scaleDeclaration   levelList
methodDeclaration  parameterList      parameter        statementList  statement
availabilityDeclaration               slotList         slot
periodList         period
constraintsDeclaration                constraintList
rulesDeclaration   ruleList           rule
planDeclaration    attributeList
expression         expressionList     factor           constant
```

---

## 4. Π — Producciones

Se usa la notación de Bison: los terminales en mayúsculas (ver §1) y los no
terminales en minúsculas. La gramática no tiene producciones λ: cada bloque vacío
admitido (`goal X { }`, `topic X { }`, `plan { }`, `[]`, `f()`) es una producción
explícita, y cada lista tiene al menos un elemento.

### 4.1 Programa

```
program          → declarationList

declarationList  → declaration
                 | declarationList declaration

declaration      → goalDeclaration
                 | topicDeclaration
                 | scaleDeclaration
                 | methodDeclaration
                 | availabilityDeclaration
                 | constraintsDeclaration
                 | rulesDeclaration
                 | planDeclaration
```

### 4.2 Objetivos y temas

```
goalDeclaration  → GOAL IDENTIFIER OPEN_BRACE memberList CLOSE_BRACE
                 | GOAL IDENTIFIER OPEN_BRACE CLOSE_BRACE

topicDeclaration → TOPIC IDENTIFIER OPEN_BRACE memberList CLOSE_BRACE
                 | TOPIC IDENTIFIER OPEN_BRACE CLOSE_BRACE
                 | TOPIC IDENTIFIER IN IDENTIFIER OPEN_BRACE memberList CLOSE_BRACE
                 | TOPIC IDENTIFIER IN IDENTIFIER OPEN_BRACE CLOSE_BRACE

memberList       → member
                 | memberList member

member           → attribute
                 | topicDeclaration

attribute        → IDENTIFIER COLON expressionList SEMICOLON
                 | METHOD COLON expressionList SEMICOLON
```

### 4.3 Escalas

```
scaleDeclaration → SCALE IDENTIFIER OPEN_BRACE levelList CLOSE_BRACE

levelList        → IDENTIFIER
                 | levelList LESS IDENTIFIER
```

### 4.4 Métodos de estudio

```
methodDeclaration → METHOD IDENTIFIER OPEN_PARENTHESIS CLOSE_PARENTHESIS
                        OPEN_BRACE statementList CLOSE_BRACE
                  | METHOD IDENTIFIER OPEN_PARENTHESIS parameterList CLOSE_PARENTHESIS
                        OPEN_BRACE statementList CLOSE_BRACE

parameterList    → parameter
                 | parameterList COMMA parameter

parameter        → IDENTIFIER COLON IDENTIFIER
                 | IDENTIFIER COLON IDENTIFIER DEFAULT expression

statementList    → statement
                 | statementList statement

statement        → SESSION expression SEMICOLON
                 | PAUSE expression SEMICOLON
                 | AFTER expression statement
                 | REPEAT expression OPEN_BRACE statementList CLOSE_BRACE
                 | FOR IDENTIFIER IN expression OPEN_BRACE statementList CLOSE_BRACE
```

Un parámetro con `default` toma ese valor cuando la invocación lo omite, así que
`Pomodoro()` y `Pomodoro(50m)` son invocaciones válidas del mismo método. El
análisis semántico exige que los parámetros con valor por defecto sean los
últimos, que el nombre del método empiece con mayúscula y que no se redefina un
método de una librería incluida.

### 4.5 Disponibilidad horaria

```
availabilityDeclaration → AVAILABILITY OPEN_BRACE slotList CLOSE_BRACE

slotList         → slot
                 | slotList slot

slot             → IDENTIFIER COLON periodList SEMICOLON

periodList       → period
                 | periodList COMMA period

period           → expression
                 | expression RANGE expression
```

### 4.6 Restricciones, reglas y plan

```
constraintsDeclaration → CONSTRAINTS OPEN_BRACE constraintList CLOSE_BRACE

constraintList   → expression SEMICOLON
                 | constraintList expression SEMICOLON

rulesDeclaration → RULES OPEN_BRACE ruleList CLOSE_BRACE

ruleList         → rule
                 | ruleList rule

rule             → WHEN expression ARROW expression SEMICOLON
                 | OTHERWISE ARROW expression SEMICOLON

planDeclaration  → PLAN OPEN_BRACE attributeList CLOSE_BRACE
                 | PLAN OPEN_BRACE CLOSE_BRACE

attributeList    → attribute
                 | attributeList attribute
```

### 4.7 Expresiones

```
expression       → expression OR expression
                 | expression AND expression
                 | NOT expression
                 | expression EQUAL expression
                 | expression NOT_EQUAL expression
                 | expression LESS expression
                 | expression GREATER expression
                 | expression LESS_EQUAL expression
                 | expression GREATER_EQUAL expression
                 | expression ADD expression
                 | expression SUB expression
                 | expression MUL expression
                 | expression DIV expression
                 | factor

factor           → OPEN_PARENTHESIS expression CLOSE_PARENTHESIS
                 | OPEN_BRACKET expressionList CLOSE_BRACKET
                 | OPEN_BRACKET CLOSE_BRACKET
                 | IDENTIFIER OPEN_PARENTHESIS expressionList CLOSE_PARENTHESIS
                 | IDENTIFIER OPEN_PARENTHESIS CLOSE_PARENTHESIS
                 | IDENTIFIER
                 | GOAL
                 | TOPIC
                 | constant

constant         → INTEGER
                 | DURATION
                 | TIME
                 | DATE
                 | STRING

expressionList   → expression
                 | expressionList COMMA expression
```

`GOAL` y `TOPIC` como factor permiten referirse al objetivo o al tema en curso
dentro de restricciones y reglas (p. ej. `finish(goal) <= deadline(goal) - 7d`).

### 4.8 Precedencia y asociatividad

Las producciones de `expression` son ambiguas por diseño; Bison las resuelve con
la siguiente tabla, de menor a mayor precedencia:

| Nivel | Operadores | Asociatividad |
| :---: | :--------- | :------------ |
| 1 | `or` | izquierda |
| 2 | `and` | izquierda |
| 3 | `not` | unario |
| 4 | `==` `!=` | izquierda |
| 5 | `<` `>` `<=` `>=` | izquierda |
| 6 | `+` `-` | izquierda |
| 7 | `*` `/` | izquierda |

Así, `topics(day) <= 2 and not load(week) > 20h` se agrupa como
`(topics(day) <= 2) and (not (load(week) > 20h))`.

---

## 5. Correspondencia con la Etapa 1

La sintaxis final se apartó de los ejemplos del informe de la Etapa 1 para que
todas las construcciones compartan una misma forma (`nombre: valor;` dentro de
bloques). Las construcciones del informe se expresan así:

| Etapa 1 | Sintaxis final | Test |
| :------ | :------------- | :--- |
| `goal TLA due 2026-12-05 priority 80%;` | `goal TLA { due: 2026-12-05; priority: alto; }` | `30-goal-priority` |
| Dificultad de 1 a 5 | Nivel de una `scale` declarada (`difficulty: medioAlto;`) | `11-scale`, `15-method-attribute` |
| `estimated 4h 30m;` | `estimated: 4h30m;` (una sola `DURATION`) | `06-literals` |
| `method pomodoro { work 25m; … }` | `include <metodos>` y `method: Pomodoro(25m, 5m, 4);`, o un `method` propio | `31-include-library`, `12-method-repeat`, `15-method-attribute` |
| `profile` + `use` | Un `method` declarado una vez (o tomado de una librería) y aplicado a varios temas | `26-reused-method`, `34-include-file` |
| `if (…) { … } else { … }` | Bloque `rules` con `when … -> …;` y `otherwise -> …;` | `19-rules` |
| `mon, wed, fri = 3h;` `sun = rest;` | `weekdays: 3h;` (un día sin declarar no está disponible para estudiar, sin `rest`) | `16`, `17`, `27-availability-free-day` |
| Sesión fija en fecha y hora | `fixed: 2026-11-20, 18:00, 90m;` | `28-fixed-session` |
| `constraint noMoreThan 2 topicsPer d;` | `constraints { topics(day) <= 2; }` | `18-constraints` |
| `plan from … until deadlines { export … }` | `plan { from: …; until: deadlines; }` (siempre se generan el HTML y el iCalendar, sin `export`) | `29-plan-range` |

Los porcentajes y booleanos del informe no tienen literal propio: la prioridad
se expresa como nivel de una escala y las condiciones como expresiones lógicas.

---

## 6. Semántica

La gramática acepta cualquier atributo con la forma `nombre: valor;`. El
análisis semántico (`src/main/c/backend/semantic-analysis`) decide cuáles
existen, qué tipo tiene cada valor y qué combinaciones tienen sentido; el
planificador (`src/main/c/backend/planning`) les da significado. Ninguno de
estos nombres es una palabra reservada.

### 6.1 Nombres

* Los objetivos y los temas comparten un único espacio de nombres: no puede
  haber dos con el mismo nombre en todo el programa.
* Los niveles de las escalas también son únicos, aunque pertenezcan a escalas
  distintas, para que `alto` signifique siempre lo mismo.
* Un método empieza con mayúscula, no se declara dos veces y no redefine un
  método de una librería incluida.
* Un tema declarado fuera de su objetivo lo indica con `in`
  (`topic Repaso in TLA { … }`), y sus subtemas pertenecen al mismo objetivo.

### 6.2 Atributos de un objetivo (`goal`)

| Atributo | Valor | Significado |
| :------- | :---- | :---------- |
| `description` | `STRING` | Texto que se muestra en la página y en el calendario. |
| `due` | `DATE` | Fecha límite. Obligatorio si el programa tiene `plan`. |
| `priority` | nivel de una escala | Prioridad, para la estrategia `priorityFirst` y para las reglas. |
| `except` | días y fechas | Días en que no se estudia ninguno de sus temas (ver §6.4). |

### 6.3 Atributos de un tema (`topic`)

| Atributo | Valor | Significado |
| :------- | :---- | :---------- |
| `description` | `STRING` | Texto que se muestra en la página y en el calendario. |
| `requires` | temas | Temas que tienen que terminar antes de empezar este. Requerir un tema con subtemas es requerir todos ellos. No se admiten ciclos. |
| `difficulty` | nivel de una escala | Dificultad, para las reglas. Los subtemas la heredan. |
| `estimated` | `DURATION` > 0 | Tiempo total de estudio. Si el tema tiene subtemas y lo declara, tiene que coincidir con la suma de ellos. |
| `method` | invocación | Método de estudio (`Pomodoro(50m)`). Los subtemas lo heredan y le gana a las reglas. |
| `fixed` | `DATE, TIME, DURATION` | Sesión fija que el planificador nunca mueve; tiene que caer dentro de la disponibilidad de ese día. |
| `daily` | `DURATION` > 0 | Tiempo de estudio por día (ver §6.4). |
| `days` | `INTEGER` > 0 | Cantidad de días de `daily`. Solo va junto con `daily`. |
| `except` | días y fechas | Días en que no se estudia el tema (ver §6.4). |

Con un `plan`, cada tema sin subtemas necesita `estimated` o `daily`.

### 6.4 Estudio diario y días excluidos

`daily` reparte un tema en días, sin dar el total:

| Forma | Significado |
| :---- | :---------- |
| `daily: 1h; days: 10;` | 1h por día durante 10 días (10h en total). |
| `daily: 1h;` | 1h por día, todos los días hasta la fecha límite. |
| `daily: 2h; estimated: 5h;` | 2h por día hasta juntar 5h (el último día, 1h). |

* Cuentan los días con disponibilidad que no están excluidos. Cada día, el
  bloque de `daily` se reserva antes que el resto de los temas, como una
  sesión fija; si no entra (por la disponibilidad o por los límites de las
  restricciones), es un error que nombra ese día.
* El método del tema arma cada día: `daily: 2h` con `Pomodoro()` son 2h de
  pomodoros. Un método que reparte sus sesiones en varios días (`after`, como
  `Espaciado`) no se puede usar con `daily`.
* Un tema con `daily` no tiene subtemas ni sesión fija, y no declara `days` y
  `estimated` a la vez.

`except` lleva una lista de días que se excluyen: días de la semana (`monday`
a `sunday`), conjuntos de ellos (`weekdays`, `weekend`) y fechas
(`except: sunday, 2026-11-15;`). Vale para cualquier tema, con `daily` o sin
él, y un `except` en un objetivo, o en un tema con subtemas, vale para todos
sus temas.

### 6.5 Disponibilidad

Cada línea de `availability` nombra un día (`monday` a `sunday`) o un
conjunto (`weekdays`, `weekend`, `every`), una sola vez. Cuando un día
pertenece a varias líneas, gana la más específica: `sunday` le gana a
`weekend`, y `weekend` a `every`. Un día que no aparece no tiene tiempo para
estudiar.

| Período | Significado |
| :------ | :---------- |
| `18:00 .. 21:00` | Ventana de las 18:00 a las 21:00. Puede haber varias, separadas por comas, sin superponerse. |
| `2h` | 2h desde el comienzo del día: las 09:00, o el `start` del plan. |

### 6.6 Plan

| Atributo | Valor | Significado |
| :------- | :---- | :---------- |
| `from` | `DATE` | Primer día del plan. Obligatorio. |
| `until` | `DATE` o `deadlines` | Último día. Sin él (o con `deadlines`), la fecha límite más lejana. |
| `start` | `TIME` | Comienzo de los días declarados como cantidad de horas (por defecto, 09:00). |
| `strategy` | `earliestDeadlineFirst` o `priorityFirst` | Qué tema se ubica primero cuando compiten por el mismo hueco. |

Un programa sin `plan` solo se valida, como una librería: no se planifica ni
genera archivos. Con un `plan`, también hacen falta `availability` y el `due`
de cada objetivo.

### 6.7 Restricciones

Dentro de `constraints` se usan las funciones del dominio:

| Función | Tipo | Significado |
| :------ | :--- | :---------- |
| `topics(day \| week)` | entero | Cantidad de temas distintos en un día o una semana. |
| `load(day \| week)` | duración | Tiempo de estudio en un día o una semana. |
| `finish(goal \| topic \| Nombre)` | fecha | Día en que termina cada objetivo, cada tema, o uno en particular. |
| `deadline(goal \| Nombre)` | fecha | Fecha límite (`due`) de un objetivo. |

Con un `plan`, cada restricción tiene una de estas formas, que el planificador
sabe cumplir:

```
topics(day) <= 2;                         // también week, y '<'
load(week) <= 20h;                        // también day, y '<'
finish(goal) <= deadline(goal) - 7d;      // adelanta la fecha límite de cada objetivo
finish(Gramaticas) < 2026-11-20;          // o la de un objetivo o tema en particular
```

### 6.8 Reglas y elección del método

Las condiciones de `rules` comparan los atributos `difficulty`, `priority` y
`estimated` del tema (`when difficulty >= alto and estimated > 4h -> …`); los
niveles se comparan dentro de su escala. El método de un tema es, en orden:

1. su `method`, o el del tema más cercano que lo contiene;
2. la acción de la primera regla que se cumple (`otherwise` se cumple siempre);
3. si no hay ninguno, bloques de 1h.

### 6.9 Métodos

Un método describe una tanda de estudio. El planificador la repite hasta cubrir
el tiempo del tema, y recorta la última sesión.

| Sentencia | Significado |
| :-------- | :---------- |
| `session 25m;` | Estudiar 25 minutos. |
| `pause 5m;` | Descansar 5 minutos (no cuenta como estudio). |
| `after 1d session 30m;` | La sentencia empieza, como mínimo, 1 día después del comienzo de la tanda. |
| `repeat 4 { … }` | Repetir las sentencias. |
| `for i in [1d, 3d] { … }` | Recorrer una lista. |

Los parámetros son de tipo `duration`, `integer` o `list`; los que tienen
`default` pueden omitirse al final de la invocación. Las sesiones seguidas,
sin `after` entre ellas, forman un bloque que se ubica entero dentro de una
ventana de la disponibilidad.

### 6.10 Planificación

El planificador es *greedy* y recorre los días del plan uno por uno. En cada
día reserva las sesiones fijas, después los bloques de `daily`, y por último
ubica los bloques del resto de los temas en el primer hueco donde entran,
respetando:

* `requires`: un tema empieza cuando terminaron los que requiere;
* los `after` de su método;
* los límites de `constraints` y los días de `except`;
* la estrategia: primero la fecha límite más cercana (o la prioridad más alta,
  con `priorityFirst`);
* la fecha límite de cada objetivo, adelantada por `finish`, y el `until` del
  plan.

Si un tema no se puede terminar a tiempo, el compilador informa cuánto le
falta y antes de qué fecha. Si el plan se puede cumplir, imprime un resumen y
escribe `plan.html` y `plan.ics` en la carpeta `out/`.
