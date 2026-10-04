[![✗](https://img.shields.io/badge/Release-v2.0.0-ffb600.svg?style=for-the-badge)](https://github.com/jbdiaz12/TP_TLA_2026C2/releases)

[![✗](https://github.com/jbdiaz12/TP_TLA_2026C2/actions/workflows/pipeline.yaml/badge.svg?branch=production)](https://github.com/jbdiaz12/TP_TLA_2026C2/actions/workflows/pipeline.yaml)

# Stujfy
Lenguaje de dominio específico para describir planes de aprendizaje y resolver
automáticamente su distribución en el tiempo.

Se declaran los objetivos con su fecha límite, los temas que los componen, las
dependencias de conocimiento entre ellos, el método de estudio aplicable a cada
uno y la disponibilidad horaria real de quien estudia. A partir de esa
descripción, el compilador construye el cronograma de sesiones. Un objetivo puede
ser rendir un examen, alcanzar un nivel de idioma o dominar cualquier disciplina
que admita ser dividida en temas.


La gramática formal `G = ⟨Σ, N, Π, S⟩` está documentada en
[`doc/GRAMMAR.md`](doc/GRAMMAR.md), y el informe de diseño de la Etapa 1 en
[`doc/TLA.- Stage 1 pdf.pdf`](doc/TLA.-%20Stage%201%20pdf.pdf).

## Estado

| Etapa | Contenido | Estado |
| :---- | :-------- | :----- |
| Stage I: Diseño | Informe en `doc/` | Entregado |
| Stage II: Frontend | Analizador léxico (Flex), sintáctico (Bison) y construcción del AST | Entregado |
| Stage III: Backend | Análisis semántico y generación del cronograma (HTML + iCal) | Pendiente |

En esta etapa el compilador acepta un programa si es léxica y sintácticamente
válido y construye su AST; todavía no valida el dominio ni genera artefactos.

## Ejemplo

```
scale nivel { bajo < medio < alto }

method pomodoro(trabajo: duration, descanso: duration, ciclos: integer) {
    repeat ciclos {
        session trabajo;
        pause descanso;
    }
}

goal TLA {
    due: 2026-12-05;

    topic Automatas  { difficulty: medio; estimated: 6h; }
    topic Gramaticas { requires: Automatas; difficulty: alto; estimated: 4h30m; }
}

availability {
    // Los días que no se declaran (aquí, el domingo) no están disponibles para estudiar.
    weekdays: 18:00 .. 21:00;
    saturday: 09:00 .. 12:00;
}

rules {
    when difficulty >= alto -> pomodoro(50m, 10m, 3);
    otherwise               -> pomodoro(25m, 5m, 4);
}

plan {
    from:     2026-09-01;
    strategy: earliestDeadlineFirst;
}
```

Un ejemplo con todas las construcciones está en
[`src/test/c/accept/22-full-program.stujfy`](src/test/c/accept/22-full-program.stujfy).

## Casos de prueba

Los programas de prueba están en `src/test/c` y se ejecutan con
`src/main/bash/test.sh` (ver [Test](#test)). Cada uno es un test de unidad que
ejercita una sola construcción, salvo `22-full-program`.

| Carpeta | Contenido | ¿Lo ejecuta `test.sh`? |
| :------ | :-------- | :--------------------- |
| `accept/` | Programas válidos; el compilador debe aceptarlos. | Sí |
| `reject/` | Programas léxica o sintácticamente inválidos; el compilador debe rechazarlos. | Sí |
| `ignore/reject/` | Programas que deben rechazarse por razones semánticas (objetivo duplicado, ciclo de dependencias, carga inviable, etc.). | No |

Los casos de `ignore/reject/` son sintácticamente válidos, por lo que hoy el
compilador los acepta (falso positivo, como prevé el enunciado para el Stage II).
Pasarán a `reject/` cuando el backend implemente el análisis semántico.

Los casos de aceptación y rechazo propuestos en el informe de la Etapa 1 se
corresponden con los siguientes programas:

| Etapa 1 | Aceptación | Rechazo |
| :-----: | :--------- | :------ |
| (I) | `23-goal-with-single-topic` | `reject/` (02 a 30) |
| (II) | `24-multiple-goals` | `ignore/reject/03-duplicate-goal` |
| (III) | `05-dependency-chain` | `ignore/reject/04-topic-in-undefined-goal` |
| (IV) | `25-two-predecessors` | `ignore/reject/05-unfeasible-load` |
| (V) | `03-nested-topics` | `ignore/reject/06-level-out-of-scale` |
| (VI) | `06-literals` | `ignore/reject/07-non-positive-duration` |
| (VII) | `12-method-repeat`, `15-method-attribute` | `ignore/reject/02-type-mismatch` |
| (VIII) | `13-method-for`, `19-rules` | `ignore/reject/08-dependency-cycle` |
| (IX) | `26-reused-method` | `ignore/reject/09-invalid-method-arguments` |
| (X) | `27-availability-free-day` | `ignore/reject/10-fixed-session-outside-availability` |

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

where `<program>` is the path to the file that represents its entry-point.

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
