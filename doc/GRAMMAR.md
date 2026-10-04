# Gramática de Stujfy

Definición formal de `G = ⟨Σ, N, Π, S⟩`. El analizador léxico
(`FlexPatterns.l`) define `Σ`; el sintáctico (`BisonGrammar.y`) define `N`, `Π` y `S`.

---

## 1. Σ — Alfabeto

### 1.1 Palabras reservadas

```
after   and     availability  constraints  for   goal    in  method
not     or      otherwise     pause        plan  repeat
rules   scale   session       topic        when
```


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

statementList    → statement
                 | statementList statement

statement        → SESSION expression SEMICOLON
                 | PAUSE expression SEMICOLON
                 | AFTER expression statement
                 | REPEAT expression OPEN_BRACE statementList CLOSE_BRACE
                 | FOR IDENTIFIER IN expression OPEN_BRACE statementList CLOSE_BRACE
```

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
| `method pomodoro { work 25m; … }` | `method pomodoro(…) { … }` y `method: pomodoro(25m, 5m, 4);` | `12-method-repeat`, `15-method-attribute` |
| `profile` + `use` | Un `method` declarado una vez y aplicado a varios temas | `26-reused-method` |
| `if (…) { … } else { … }` | Bloque `rules` con `when … -> …;` y `otherwise -> …;` | `19-rules` |
| `mon, wed, fri = 3h;` `sun = rest;` | `weekdays: 3h;` `sunday: rest;` | `16`, `17`, `27-availability-rest-day` |
| Sesión fija en fecha y hora | `fixed: 2026-11-20, 18:00, 90m;` | `28-fixed-session` |
| `constraint noMoreThan 2 topicsPer d;` | `constraints { topics(day) <= 2; }` | `18-constraints` |
| `plan from … until deadlines { export … }` | `plan { from: …; until: deadlines; export: calendar, summary; }` | `29-plan-export` |

Los porcentajes y booleanos del informe no tienen literal propio: la prioridad
se expresa como nivel de una escala y las condiciones como expresiones lógicas.
