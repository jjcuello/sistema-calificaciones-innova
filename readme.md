# Instituto Innova — Sistema de gestión de calificaciones

## Evaluación 1 · Corte 2

**Exposición: Enfoque Orientado a Objetos (EOO)**

| Dato | Información |
| --- | --- |
| Estudiante | José Javier Cuello Camargo |
| Profesor | Fernando Buelvas |
| Institución | Instituto Politécnico Santiago Mariño, Extensión Caracas |
| Caso | 7 — Sistema de gestión de calificaciones |
| Objetivo | Analizar y exponer el enfoque orientado a objetos desde un caso práctico. |

> **Estado del proyecto:** documentación y propuesta de diseño. La aplicación en C++ y la interfaz web todavía no están implementadas. Las clases, operaciones y tecnologías descritas son propuestas, no funcionalidades disponibles.

## 1. Contexto del caso

La institución educativa **Instituto Innova** desea migrar su sistema de calificaciones, actualmente basado en hojas de cálculo y archivos sueltos, a una plataforma orientada a objetos.

El sistema debe permitir:

- Registrar estudiantes, profesores, cursos y asignaturas.
- Matricular a un estudiante en múltiples cursos.
- Asignar a cada curso un profesor, un horario y un aula, con un máximo de **40 estudiantes por sección**.
- Permitir que los profesores registren notas parciales, prácticas y exámenes finales.
- Calcular automáticamente el promedio ponderado: **20 % parciales, 30 % prácticas y 50 % examen final**.
- Soportar cursos regulares, cursos taller con evaluación **100 % práctica** y cursos teórico-prácticos.
- Generar reportes de rendimiento por estudiante, curso y semestre.
- Emitir alertas automáticas cuando el promedio de un estudiante sea **menor que 11**.
- Permitir que los estudiantes consulten sus notas en línea.
- Permitir que los administradores generen reportes consolidados por semestre y programa académico.

El equipo de desarrollo propone el EOO para representar las entidades del dominio, reutilizar comportamientos y facilitar el mantenimiento.

## 2. Parte 1 — Componentes principales del EOO

### Pregunta 1 — Clases y objetos

Una **clase** define el estado y el comportamiento de un tipo de entidad. Un **objeto** es una instancia concreta de esa clase, con identidad y valores propios.

Para este diseño, cada objeto de curso representa una **sección ofertada de una asignatura en un semestre**. Así, dos secciones de la misma asignatura pueden tener distintos profesores, horarios, aulas y matrículas.

| Clase | Atributos relevantes | Métodos propuestos | ¿Por qué es una clase y no un simple dato? |
| --- | --- | --- | --- |
| `Estudiante` | `id`, `nombre`, `programaAcademico` | `consultarNotas()`, `consultarCursos()` | Tiene identidad, participa en matrículas y permite consultar información académica asociada; no se limita a almacenar un nombre. |
| `Profesor` | `id`, `nombre`, `especialidad` | `registrarNota()`, `consultarCursosAsignados()` | Representa a una persona con responsabilidades y operaciones propias, sujetas a la autorización sobre sus cursos. |
| `Curso` | `id`, `asignatura`, `profesor`, `horario`, `aula`, `semestre`, `matriculas` | `matricularEstudiante()`, `registrarNota()`, `calcularPromedio()` | Agrupa información y reglas: controla el cupo, las matrículas y la evaluación de sus estudiantes. |

**Ejemplos de objetos con datos ficticios:**

- Un estudiante con identificador `EST-001`, llamado Ana Pérez.
- Un profesor con identificador `PROF-001`, llamado Luis Rojas.
- Un curso regular `CUR-001`, correspondiente a Programación I, sección A, semestre `2026-2`.

`Curso` será una clase base abstracta; los objetos concretos se crearán como `CursoRegular`, `CursoTaller` o `CursoTeoricoPractico`.

**Clases de apoyo previstas:**

- `Asignatura`: identifica la materia del plan de estudios; no debe confundirse con su sección ofertada.
- `Matricula`: relaciona un estudiante con un curso y mantiene sus evaluaciones en ese curso.
- `Evaluacion`: representa una calificación y su categoría.
- `Semestre` y `ProgramaAcademico`: permiten organizar las consultas y los reportes consolidados.
- `ServicioReportes`: reúne información del dominio para construir reportes sin mezclar esa tarea con la interfaz web.

La operación del profesor para registrar una nota debe delegar en el curso correspondiente. La validación no debe duplicarse en ambas clases.

### Pregunta 2 — Encapsulamiento

**Clase seleccionada: `Curso`.**

#### Atributos privados

- **`matriculas`:** evita agregar estudiantes directamente, duplicar inscripciones o superar el límite de 40 estudiantes.
- **Evaluaciones almacenadas en cada matrícula:** evita modificar notas sin validar su valor, categoría, estudiante y profesor responsable. La protección debe mantenerse también dentro de `Matricula`.
- **`profesor`:** evita cambiar al responsable de un curso sin pasar por una operación autorizada.
- **`asignatura`, `semestre`, `horario` y `aula`:** evita modificaciones arbitrarias que dejen al curso en un estado inconsistente.

El máximo de **40 estudiantes** debe representarse como una constante de la regla de negocio, no como un valor modificable libremente desde la web.

#### Interfaz pública propuesta

| Método | Responsabilidad |
| --- | --- |
| `matricularEstudiante()` | Comprobar que el estudiante no esté matriculado y que haya cupo antes de inscribirlo. |
| `asignarProfesor()` | Cambiar el profesor mediante el flujo administrativo autorizado. |
| `registrarNota()` | Validar la matrícula, la categoría, el rango de la nota y la autorización del profesor antes de registrar o actualizar la evaluación. |
| `consultarNotas()` | Entregar una consulta de solo lectura, respetando los permisos del usuario. |
| `calcularPromedio()` | Aplicar la regla de evaluación del tipo de curso. |

No se expondrán listas modificables ni un método genérico que permita sobrescribir todo el estado. La identidad del usuario deberá obtenerse de la sesión autenticada, no confiarse a un identificador enviado libremente por el navegador.

#### Beneficio de ocultar la implementación

La interfaz web podrá solicitar una matrícula sin conocer cómo se almacenan internamente los estudiantes. Si posteriormente cambia la estructura de datos o la persistencia, el contrato público puede mantenerse.

Además, todas las entradas al sistema deberán respetar las mismas reglas. Por ejemplo, no será posible eludir el límite de cupos modificando directamente la colección de matrículas.

### Pregunta 3 — Herencia y polimorfismo

#### Clase principal: `Curso`

La clase base concentrará:

- **Atributos comunes:** identificador, asignatura, profesor, horario, aula, semestre y matrículas.
- **Comportamientos comunes:** matricular estudiantes, controlar cupos, consultar matrículas y validar el registro de notas.
- **Contrato de evaluación:** `calcularPromedio()`, cuya implementación dependerá del tipo de curso.

En C++, el cálculo se declarará como un método **virtual puro**, las clases hijas lo implementarán con `override` y la clase base tendrá un **destructor virtual**.

#### Clases hijas

| Clase hija | Comportamiento específico |
| --- | --- |
| `CursoRegular` | Calcula el promedio con 20 % de parciales, 30 % de prácticas y 50 % de examen final. |
| `CursoTaller` | Calcula el promedio exclusivamente a partir de las prácticas; no exige parciales ni examen final. |
| `CursoTeoricoPractico` | Debe aplicar una regla que combine teoría y práctica. El enunciado no define una ponderación diferenciada: debe confirmarse antes de implementar su cálculo. |

No se inventará una distribución para `CursoTeoricoPractico`. Si se confirma que utiliza la misma regla que un curso regular, convendrá reutilizar ese cálculo y evitar lógica duplicada. La especialización solo debe añadir reglas que realmente existan.

#### Ejemplo concreto de polimorfismo

Un generador de reportes recorre cursos mediante referencias a la clase base `Curso` y solicita `calcularPromedio()` para una matrícula:

- Si el objeto es un `CursoRegular`, se ejecuta el cálculo ponderado.
- Si el objeto es un `CursoTaller`, se ejecuta el cálculo basado únicamente en prácticas.
- Si el objeto es un `CursoTeoricoPractico`, se ejecutará su regla una vez definida.

El generador no necesita preguntar por el tipo de curso con una cadena de condiciones. La llamada es la misma, pero el comportamiento depende del objeto concreto.

**Ejemplo numérico:**

| Tipo de curso | Calificaciones | Resultado |
| --- | --- | --- |
| Regular | Parciales: 12; prácticas: 14; final: 16 | `12 × 0,20 + 14 × 0,30 + 16 × 0,50 = 14,6` |
| Taller | Prácticas: 14 | `14 × 1,00 = 14` |

## 3. Parte 2 — Análisis crítico

### Ventajas del EOO frente al enfoque estructurado

Un enfoque estructurado también puede resolver este problema y resultar rápido para un prototipo pequeño. La defensa del EOO no consiste en afirmar que las funciones sean incapaces de hacerlo, sino en justificar cómo se organizará un sistema con entidades relacionadas y reglas variables.

1. **Protección de reglas y consistencia de los datos.**  
   El curso controla sus matrículas y el acceso a las notas. Esto reduce el riesgo de que distintos formularios apliquen reglas diferentes, permitan un estudiante número 41 o acepten cambios de notas no autorizados.

2. **Reutilización y extensión de comportamientos.**  
   Los cursos regulares y los talleres comparten la gestión de profesor, horario, aula y estudiantes. Solo especializan lo que cambia: la evaluación. Un nuevo tipo de curso puede incorporarse sin reescribir el proceso de generación de reportes.

3. **Mantenimiento y pruebas por responsabilidad.**  
   El cálculo académico puede probarse sin abrir el navegador ni desplegar la aplicación. Si cambia una ponderación, se modifica la regla correspondiente sin alterar la presentación de notas o el registro de estudiantes.

**Conclusión:** el EOO se elige por su adecuación al dominio y a la evolución de sus reglas, no por una supuesta superioridad en velocidad de ejecución. Estos beneficios requieren un diseño disciplinado; usar clases por sí solo no los garantiza.

## 4. Reglas confirmadas y decisiones pendientes

### Reglas explícitas del caso

- Máximo de **40 estudiantes por sección**.
- Un estudiante puede matricularse en varios cursos.
- Cada curso tiene un profesor asignado, un horario y un aula.
- Ponderación general: **20 % parciales, 30 % prácticas y 50 % examen final**.
- Los talleres se evalúan **100 % mediante prácticas**.
- El umbral de riesgo es **promedio < 11**; un promedio exactamente igual a 11 no está por debajo del umbral.

### Propuestas que deben validarse antes de programar

| Tema | Propuesta o aclaración pendiente |
| --- | --- |
| Escala de notas | Usar de 0 a 20, con decimales. El enunciado fija el umbral de 11, pero no declara expresamente el rango. |
| Varias notas por categoría | Usar la media aritmética dentro de cada categoría y después aplicar su ponderación. Confirmar si cada actividad tiene un peso propio. |
| Evaluaciones pendientes | Mostrar el promedio final como pendiente hasta completar las evaluaciones requeridas. No convertir automáticamente una nota ausente en cero. |
| Cursos teórico-prácticos | Confirmar la distribución entre teoría y práctica y cómo se relaciona con la ponderación general. |
| Alcance de las alertas | Proponer una alerta por promedio final de cada curso; confirmar si también se exige promedio global, semestral o provisional. |
| Canal de alertas | Para la demostración, mostrar avisos automáticos en la web al recalcular notas. Correo y notificaciones externas no forman parte de esta propuesta inicial. |
| Redondeo | Comparar el promedio sin redondear contra 11 y redondear solo su presentación. Confirmar la norma académica. |
| Reportes consolidados | Confirmar cómo agregar resultados de varios cursos: promedio simple o ponderado por créditos, y tratamiento de cursos pendientes. |

Estas propuestas no deben presentarse como requisitos dados por el profesor.

## 5. Propuesta técnica: C++ y una web sencilla

### Render y FastAPI cumplen funciones distintas

- **Render** es una plataforma de alojamiento donde se puede desplegar el servicio.
- **FastAPI** es un framework de Python para construir APIs; no sustituye al alojamiento.
- **Crow** es un framework de C++ para crear servicios HTTP y puede exponer el modelo del proyecto directamente a una interfaz web.

### Opción recomendada: C++ + Crow + HTML/CSS/JavaScript

```text
Navegador: HTML, CSS y JavaScript
                 |
              HTTP/JSON
                 |
         API en C++ con Crow
                 |
       Modelo orientado a objetos
                 |
       Repositorio de datos
```

**Despliegue propuesto:** empaquetar el servicio y la interfaz en Docker y alojarlos en Render.

Esta opción mantiene las clases, las reglas académicas y el servidor en C++, evitando un segundo lenguaje en el backend. La interfaz puede servirse desde el mismo servicio para simplificar la demostración.

- **C++17 y CMake:** propuesta para implementar y compilar el proyecto.
- **Crow:** capa HTTP; las rutas delegarán en el modelo, sin duplicar las fórmulas.
- **HTML, CSS y JavaScript:** interfaz pequeña, sin un framework de frontend inicialmente.
- **Datos ficticios en memoria:** primera demostración, indicando expresamente que se reinician al cerrar o reiniciar el servicio.
- **Persistencia posterior:** evaluar SQLite con almacenamiento persistente, o una base de datos externa. No considerar el sistema de archivos del contenedor como garantía de conservación de datos.

### Alternativa: C++ + FastAPI + web

Es posible mantener el dominio en C++ y utilizar FastAPI como capa HTTP en Python. Para una demostración pequeña, esa capa podría invocar un ejecutable C++ mediante un contrato JSON de entrada y salida.

Sin embargo, esta alternativa añade integración entre procesos, compilación del ejecutable, manejo de errores y una decisión explícita sobre dónde persisten los datos entre peticiones. Las ponderaciones y reglas deben seguir en C++, no reimplementarse en Python.

**Recomendación:** empezar con C++ y Crow para que el trabajo evidencie directamente el EOO en el lenguaje elegido. Considerar FastAPI si existe un motivo concreto para introducir Python.

## 6. Alcance propuesto de la demostración

1. Registrar estudiantes, profesores, asignaturas y secciones de cursos.
2. Matricular estudiantes y demostrar el control de cupos.
3. Registrar evaluaciones y calcular promedios según el tipo de curso.
4. Consultar notas desde una vista de estudiante.
5. Mostrar alertas de riesgo académico.
6. Consultar reportes por estudiante, curso y semestre, con consolidación por programa académico.

Las vistas de estudiante, profesor y administrador tendrán responsabilidades distintas. Si inicialmente se permite cambiar de rol para la exposición, se identificará como **simulación**, no como autenticación real. Un despliegue con información académica real requerirá autenticación y autorización verificadas en el servidor.

### Casos de verificación previstos

| Caso | Resultado esperado |
| --- | --- |
| Matricular al estudiante número 40 | Operación permitida. |
| Intentar matricular al estudiante número 41 | Rechazo explícito, sin modificar las matrículas existentes. |
| Matricular dos veces al mismo estudiante en una sección | Rechazo explícito de la matrícula duplicada. |
| Matricular al mismo estudiante en otra sección de otra asignatura | Operación permitida. |
| Curso regular con parciales 12, prácticas 14 y final 16 | Promedio 14,6. |
| Taller con práctica 14 y sin parciales ni examen final | Promedio 14. |
| Promedio 10,9 frente a promedio 11 | Alerta solo para 10,9, según el alcance de alertas que se confirme. |
| Nota fuera del rango acordado | Rechazo explícito, sin sobrescribir una nota válida. |
| Falta una evaluación requerida | Resultado pendiente, según la política que se confirme. |
| Profesor que intenta modificar un curso ajeno | Rechazo por falta de autorización. |
| Reporte de un semestre o programa específico | Incluye únicamente los registros del filtro y aplica la regla de consolidación acordada. |

Estos casos son criterios de aceptación previstos; todavía no existen pruebas automatizadas ni resultados de ejecución.

## 7. Orden de trabajo sugerido

1. Validar las reglas pendientes y el alcance de la exposición.
2. Diseñar las clases y sus relaciones mediante un diagrama UML.
3. Implementar y probar el modelo en C++, inicialmente sin interfaz web.
4. Incorporar la API y una interfaz sencilla.
5. Verificar reportes, permisos, alertas y comportamiento de los datos al reiniciar.
6. Preparar Docker, desplegar y documentar los comandos reales de ejecución.

## 8. Referencias técnicas

- [Crow: framework web para C++](https://crowcpp.org/master/).
- [Render: despliegues mediante Docker](https://render.com/docs/docker).
- [FastAPI: framework para APIs en Python](https://fastapi.tiangolo.com/).
