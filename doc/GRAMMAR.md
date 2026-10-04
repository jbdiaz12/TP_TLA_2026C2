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


## ver de agregar N, Π y el readme si poner mas cosas 


