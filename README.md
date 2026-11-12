[![✗](https://img.shields.io/badge/Release-v3.0.0-ffb600.svg?style=for-the-badge)](https://github.com/jbdiaz12/TP_TLA_2026C2/releases)

[![✗](https://github.com/jbdiaz12/TP_TLA_2026C2/actions/workflows/pipeline.yaml/badge.svg?branch=production)](https://github.com/jbdiaz12/TP_TLA_2026C2/actions/workflows/pipeline.yaml)

# Stujfy
Lenguaje de dominio específico para describir planes de aprendizaje y resolver
automáticamente su distribución en el tiempo.

Se declaran los objetivos con su fecha límite, los temas que los componen, las
dependencias de conocimiento entre ellos, el método de estudio aplicable a cada
uno y la disponibilidad horaria real de quien estudia. A partir de esa
descripción, el compilador construye el cronograma de sesiones y lo genera como
una página web y como un calendario para importar en Google Calendar, Apple o
Outlook. Un objetivo puede ser rendir un examen, alcanzar un nivel de idioma o
dominar cualquier disciplina que admita ser dividida en temas.

La gramática formal `G = ⟨Σ, N, Π, S⟩` y la semántica de cada construcción
están documentadas en [`doc/GRAMMAR.md`](doc/GRAMMAR.md), y el informe de diseño
de la Etapa 1 en [`doc/TLA.- Stage 1 pdf.pdf`](doc/TLA.-%20Stage%201%20pdf.pdf).

## Estado

| Etapa | Contenido | Estado |
| :---- | :-------- | :----- |
| Stage I: Diseño | Informe en `doc/` | Entregado |
| Stage II: Frontend | Analizador léxico (Flex), sintáctico (Bison) y construcción del AST | Entregado |
| Stage III: Backend | Análisis semántico, planificación y generación del cronograma (HTML + iCal) | Completo |

El compilador analiza el programa léxica, sintáctica y semánticamente,
planifica las sesiones de estudio y escribe el cronograma en `out/`. Si el plan
no se puede cumplir, informa qué tema no llega a tiempo y por qué.

## Ejemplo

```
include <metodos>

scale nivel { bajo < medio < alto }

goal TLA {
    description: "Segundo parcial de Teoría de Lenguajes";
    due: 2026-12-05;
    priority: alto;

    topic Automatas  { difficulty: medio; estimated: 6h; }
    topic Gramaticas { requires: Automatas; difficulty: alto; estimated: 4h30m; }
}

goal Ingles {
    due: 2026-12-20;
    except: weekend;                          // Inglés no se estudia los fines de semana

    topic Vocabulario { daily: 30m; days: 20; method: Bloque(); }  // 30m por día, durante 20 días
}

availability {
    // Los días que no se declaran (aquí, el domingo) no están disponibles para estudiar.
    weekdays: 18:00 .. 21:00;
    saturday: 09:00 .. 12:00;
}

constraints {
    load(week) <= 12h;
    finish(goal) <= deadline(goal) - 7d;      // cada objetivo, listo una semana antes
}

rules {
    when difficulty >= alto -> Espaciado();
    otherwise               -> Pomodoro();
}

plan {
    from: 2026-09-01;
}
```

El compilador imprime un resumen del cronograma y escribe dos archivos:

```
Agenda del 2026-09-01 al 2026-09-28: 44 sesiones, 20h30m de estudio.
  Automatas     15 sesiones (6h), del 2026-09-01 al 2026-09-04; límite: 2026-11-28.
  Gramaticas     9 sesiones (4h30m), del 2026-09-04 al 2026-09-25; límite: 2026-11-28.
  Vocabulario   20 sesiones (10h), del 2026-09-01 al 2026-09-28; límite: 2026-12-13.
Generé la página 'out/plan.html' y el calendario 'out/plan.ics'.
```

Hay ejemplos más completos en
[`src/test/c/accept/22-full-program.stujfy`](src/test/c/accept/22-full-program.stujfy)
y [`src/test/c/accept/56-cuatrimestre.stujfy`](src/test/c/accept/56-cuatrimestre.stujfy).

## Salida

| Archivo | Contenido |
| :------ | :-------- |
| `out/plan.html` | Página autocontenida (sin recursos externos): las horas de cada objetivo y tema, el grafo de dependencias entre los temas y la agenda día por día, con cada sesión y pausa. Se adapta al celular y al modo oscuro. |
| `out/plan.ics` | Calendario iCalendar (RFC 5545), con un evento por bloque de estudio. Las horas no tienen zona horaria: se toman en la del calendario que lo importa. |

Como el contenedor comparte la carpeta del proyecto, los archivos aparecen
también fuera de él. En macOS:

```bash
open out/plan.html             # abre la página en el navegador
qlmanage -p out/plan.ics       # muestra los eventos sin importarlos
```

Para importar el calendario: en Apple Calendar, *Archivo → Importar*; en Google
Calendar, *Configuración → Importar y exportar*. Conviene importarlo en un
calendario aparte, para poder borrarlo entero.

## Construcciones principales

| Construcción | Ejemplo | Qué hace |
| :----------- | :------ | :------- |
| Objetivo | `goal TLA { due: 2026-12-05; }` | Algo que se estudia para una fecha límite. |
| Tema | `topic LR { requires: LL1; estimated: 2h; }` | Una parte de un objetivo, con su tiempo y sus dependencias. |
| Estudio diario | `daily: 1h; days: 10; except: sunday;` | 1h por día durante 10 días, sin dar el total y salteando los domingos. |
| Sesión fija | `fixed: 2026-11-20, 18:00, 90m;` | Una clase o consulta que no se mueve. |
| Escala | `scale nivel { bajo < medio < alto }` | Niveles para la dificultad y la prioridad. |
| Método | `method Mio(t: duration) { session t; pause 5m; }` | Cómo se arma una tanda de estudio. |
| Disponibilidad | `weekdays: 18:00 .. 21:00;` | Cuándo hay tiempo para estudiar. |
| Restricciones | `load(day) <= 4h;` `finish(goal) <= deadline(goal) - 7d;` | Límites que el plan respeta. |
| Reglas | `when difficulty >= alto -> Espaciado();` | Qué método usa cada tema. |
| Plan | `plan { from: 2026-09-01; strategy: priorityFirst; }` | Desde cuándo y cómo se planifica. |

La sección 6 de [`doc/GRAMMAR.md`](doc/GRAMMAR.md#6-semántica) detalla cada
atributo, los valores que admite y cómo los usa el planificador.

## Librería estándar

Como en C, un programa puede incluir librerías escritas en el propio Stujfy. La
directiva no lleva `;`:

```
include <metodos>              // librería estándar
include "mis-metodos.stujfy"   // archivo propio, relativo a la carpeta desde la que se ejecuta el compilador
```

La librería estándar está en [`src/main/stujfy/lib`](src/main/stujfy/lib) y
trae `<metodos>`:

| Método | Parámetros (valor por defecto) | Qué hace |
| :----- | :----------------------------- | :------- |
| `Bloque` | `duracion` (`1h`) | Una sesión continua. |
| `Pomodoro` | `trabajo` (`25m`), `descanso` (`5m`), `ciclos` (`4`) | Bloques de trabajo separados por descansos cortos. |
| `Espaciado` | `unidad` (`30m`), `intervalos` (`[1d, 3d, 7d, 21d]`) | Una sesión y repasos a intervalos crecientes. |
| `ActiveRecall` | `estudio` (`25m`), `evocacion` (`10m`), `rondas` (`2`) | Estudiar el material y reconstruirlo sin mirarlo. |

Los argumentos van por posición, y los que se omiten al final toman su valor por
defecto: `Pomodoro()` es el pomodoro clásico y `Pomodoro(50m)` cambia solo el
trabajo. Un método de la librería no se puede redefinir, porque su cuerpo es lo
que lo hace ser ese método; para estudiar de otra forma, se declara un `method`
propio con otro nombre. Los nombres de los métodos empiezan con mayúscula.

## Casos de prueba

Los programas de prueba están en `src/test/c` y se ejecutan con
`src/main/bash/test.sh` (ver [Test](#test)). Cada uno es un test de unidad que
ejercita una sola construcción o un solo error, salvo los programas completos
(`22-full-program` y `56-cuatrimestre`).

| Carpeta | Contenido |
| :------ | :-------- |
| `accept/` | Programas válidos; el compilador debe aceptarlos y, si tienen `plan`, generar el cronograma. |
| `reject/` | Programas con errores léxicos, sintácticos, semánticos o de planificación; el compilador debe rechazarlos. |
| `ignore/reject/` | Vacía: los casos que esperaban validaciones del backend pasaron a `reject/`. |

`src/test/c/lib/` contiene archivos auxiliares que incluyen los tests, y no son
tests en sí.

Los casos de aceptación y rechazo propuestos en el informe de la Etapa 1 se
corresponden con los siguientes programas:

| Etapa 1 | Aceptación | Rechazo |
| :-----: | :--------- | :------ |
| (I) | `23-goal-with-single-topic` | `reject/` (02 a 30) |
| (II) | `24-multiple-goals` | `42-duplicate-goal` |
| (III) | `05-dependency-chain` | `51-topic-in-undefined-goal` |
| (IV) | `25-two-predecessors` | `139-unfeasible-load` |
| (V) | `03-nested-topics` | `57-level-out-of-scale` |
| (VI) | `06-literals` | `80-non-positive-duration` |
| (VII) | `12-method-repeat`, `15-method-attribute` | `72-type-mismatch` |
| (VIII) | `13-method-for`, `19-rules` | `62-dependency-cycle` |
| (IX) | `26-reused-method` | `111-invalid-method-arguments` |
| (X) | `27-availability-free-day` | `125-fixed-session-outside-availability` |

La sección 5 de [`doc/GRAMMAR.md`](doc/GRAMMAR.md#5-correspondencia-con-la-etapa-1)
detalla cómo cambió la sintaxis respecto de los ejemplos del informe.


## Equipo
| Nombres | Apellidos | Legajo | E-mail |
| :------ | :-------- | :----- | :----- |
| Jesús Gabriel | Díaz | 64475 | jbastidasdiaz@itba.edu.ar |
| Francesco Vega | Scarabino | 65199 | fvegascarabino@itba.edu.ar |


## Contenido

* [Estado](#estado)
* [Ejemplo](#ejemplo)
* [Salida](#salida)
* [Construcciones principales](#construcciones-principales)
* [Librería estándar](#librería-estándar)
* [Casos de prueba](#casos-de-prueba)
* [Requirements](#requirements)
* [Configuration](#configuration)
* [Commands](#commands)
* [CI/CD](#cicd)
* [Recommended Extensions](#recommended-extensions)

## Requirements

* [Docker v28.3.2](https://www.docker.com/)

## Configuration

Set the following environment variables to control and configure the behaviour of the application:

| Name                  | Default | Description                                                                                                                                                           |
| :-------------------- | :-----: | :-------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `ENVIRONMENT`         | `Local` | The active environment name. The available environments are: `Local`, `Development` and `Production`.                                                                 |
| `LOG_IGNORED_LEXEMES` | `true`  | When `true`, logs all of the ignored lexemes found with Flex at `DEBUGGING` level. To remove those logs from the console output set it to `false`.                    |
| `LOGGING_LEVEL`       | `ALL`   | The minimum level to log in the console output. From lower to higher, the available levels are: `ALL`, `DEBUGGING`, `INFORMATION`, `WARNING`, `ERROR` and `CRITICAL`. |
| `STUJFY_LIBRARY_PATH` | `src/main/stujfy/lib` | The folder where `include <name>` looks for `name.stujfy`. The default value is relative to the root of the repository, where the scripts run the compiler. |
| `STUJFY_OUTPUT_PATH` | `out` | The folder where the compiler writes `plan.html` and `plan.ics`. It's created if it doesn't exist. |

_Docker Compose_ can read the variables from an `.env` file too (see `compose.yaml` file).

## Commands

### Start

Rises an ephemeral container, ready to start development:

```bash
docker compose run --rm compiler
```

### Build

Builds or rebuilds the entire compiler:

```bash
src/main/bash/build.sh
```

### Run

Compiles a program:

```bash
src/main/bash/run.sh <program>
```

where `<program>` is the path to the file that represents its entry-point. If the program has a `plan`, the compiler prints a summary of the schedule and writes `out/plan.html` and `out/plan.ics` (see [Salida](#salida)). To see only the summary and the errors, without the logs:

```bash
LOGGING_LEVEL=ERROR src/main/bash/run.sh <program>
```

### Test

Executes every available unit-test under `src/test/c` folder:

```bash
src/main/bash/test.sh
```

### Stop

Logout, destroy the ephemeral containers and shutdowns the cluster:

```bash
exit
docker compose down
```

### Docker

| Command                                 | Description                                             |
| :-------------------------------------- | :------------------------------------------------------ |
| `docker builder prune --all`            | Removes all builds and complete build cache.            |
| `docker compose --progress=plain build` | Forces a build or rebuild of the images in the cluster. |
| `docker image prune`                    | Removes all of the dangling images from Docker.         |
| `docker network prune`                  | Removes unused networks from Docker.                    |
| `docker volume prune`                   | Removes unused volumes from Docker.                     |

## CI/CD

To trigger an automatic integration on every push or PR (_Pull Request_), you must activate _GitHub Actions_ in the _Settings_ tab. Use the following configuration:

| Key                                                        | Value                                               |
| :--------------------------------------------------------- | :-------------------------------------------------- |
| `Actions permissions`                                      | `Allow all actions and reusable workflows`          |
| `Allow GitHub Actions to create and approve pull requests` | `false`                                             |
| `Artifact and log retention`                               | `30 days`                                           |
| `Fork pull request workflows from outside collaborators`   | `Require approval for all outside collaborators`    |
| `Workflow permissions`                                     | `Read repository contents and packages permissions` |

## Recommended Extensions

* [C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools)
* [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools)
* [Yash](https://marketplace.visualstudio.com/items?itemName=daohong-emilio.yash)
