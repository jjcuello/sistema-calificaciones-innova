import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { once } from "node:events";
import { createServer } from "node:net";
import path from "node:path";
import { setTimeout as esperar } from "node:timers/promises";
import { fileURLToPath } from "node:url";

const ejecutable = process.argv[2];
if (!ejecutable) {
    throw new Error("Indica la ruta del ejecutable como argumento.");
}
const proyecto = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");

async function puertoDisponible() {
    const servidor = createServer();
    servidor.listen(0, "127.0.0.1");
    await once(servidor, "listening");
    const direccion = servidor.address();
    const puerto = direccion.port;
    await new Promise((resolver, rechazar) => servidor.close((error) => error ? rechazar(error) : resolver()));
    return puerto;
}

async function probarPuertoInvalido() {
    const proceso = spawn(ejecutable, [], {
        cwd: proyecto,
        env: { ...process.env, PORT: "no-valido" },
        windowsHide: true
    });
    let errores = "";
    proceso.stderr.on("data", (datos) => { errores += datos; });
    const [codigo] = await once(proceso, "close");
    assert.equal(codigo, 1);
    assert.match(errores, /PORT/);
}

async function iniciarServidor(puerto) {
    const proceso = spawn(ejecutable, [], {
        cwd: proyecto,
        env: { ...process.env, PORT: String(puerto) },
        windowsHide: true
    });
    let salida = "";
    proceso.stdout.on("data", (datos) => { salida += datos; });
    proceso.stderr.on("data", (datos) => { salida += datos; });
    const limite = Date.now() + 15000;
    let ultimoError;
    while (Date.now() < limite) {
        if (proceso.exitCode !== null) {
            throw new Error(`El servidor terminó antes de iniciar: ${salida}`);
        }
        try {
            const respuesta = await fetch(`http://127.0.0.1:${puerto}/health`);
            if (respuesta.ok) {
                return proceso;
            }
            ultimoError = new Error(`La verificación respondió HTTP ${respuesta.status}.`);
        } catch (error) {
            ultimoError = error;
        }
        await esperar(100);
    }
    proceso.kill();
    throw new Error(`El servidor no inició: ${salida}`, { cause: ultimoError });
}

async function ejecutar() {
    await probarPuertoInvalido();
    const puerto = await puertoDisponible();
    const base = `http://127.0.0.1:${puerto}`;
    const servidor = await iniciarServidor(puerto);

    async function peticion(ruta, metodo = "GET", datos, codigo = 200) {
        const respuesta = await fetch(base + ruta, {
            method: metodo,
            headers: datos === undefined ? {} : { "Content-Type": "application/json" },
            body: datos === undefined ? undefined : JSON.stringify(datos)
        });
        const contenido = await respuesta.json();
        assert.equal(respuesta.status, codigo, JSON.stringify(contenido));
        return contenido;
    }

    async function crear(ruta, datos) {
        return (await peticion(ruta, "POST", datos, 201)).id;
    }

    try {
        const pagina = await fetch(base + "/");
        assert.equal(pagina.status, 200);
        assert.match(await pagina.text(), /Demostración pública con roles simulados/);
        assert.equal((await fetch(base + "/styles.css")).status, 200);
        assert.equal((await fetch(base + "/app.js")).status, 200);

        const inicial = await peticion("/api/estado");
        assert.equal(inicial.estudiantes.length, 3);
        assert.equal(inicial.cursos.length, 3);
        assert.equal(inicial.cursos[0].matriculas[0].promedio, 14.6);

        const profesor = await crear("/api/profesores", { nombre: "Docente", especialidad: "Sistemas" });
        const otroProfesor = await crear("/api/profesores", { nombre: "Otro", especialidad: "Sistemas" });
        const asignatura = await crear("/api/asignaturas", { nombre: "Evaluación de prueba" });
        const exacto = await crear("/api/estudiantes", { nombre: "Exacto", programa: "Programa de prueba" });
        const inferior = await crear("/api/estudiantes", { nombre: "Inferior", programa: "Otro programa" });
        const curso = await crear("/api/cursos", {
            tipo: "regular", asignaturaId: asignatura, profesorId: profesor,
            semestre: "2030-1", horario: "Lunes", aula: "A1"
        });
        for (const estudianteId of [exacto, inferior]) {
            await peticion(`/api/cursos/${curso}/matriculas`, "POST", { estudianteId }, 201);
        }
        await peticion(`/api/cursos/${curso}/matriculas`, "POST", { estudianteId: exacto }, 409);
        await peticion(`/api/cursos/${curso}/notas`, "POST", {
            profesorId: otroProfesor, estudianteId: exacto, categoria: "parcial", nota: 10
        }, 403);
        await peticion(`/api/cursos/${curso}/notas`, "POST", {
            profesorId: profesor, estudianteId: exacto, categoria: "parcial", nota: 21
        }, 400);
        await peticion("/api/estudiantes", "POST", { nombre: 42, programa: "Sistemas" }, 400);

        for (const [estudianteId, parcial] of [[exacto, 1.7], [inferior, 1.699999]]) {
            for (const [categoria, nota] of [["parcial", parcial], ["practica", 8.2], ["examen_final", 16.4]]) {
                await peticion(`/api/cursos/${curso}/notas`, "POST", {
                    profesorId: profesor, estudianteId, categoria, nota
                }, 201);
            }
        }
        const actualizado = await peticion("/api/estado");
        const matriculas = actualizado.cursos.find((actual) => actual.id === curso).matriculas;
        const notaExacta = matriculas.find((actual) => actual.estudianteId === exacto);
        const notaInferior = matriculas.find((actual) => actual.estudianteId === inferior);
        assert.equal(notaExacta.promedio, 11);
        assert.equal(notaExacta.riesgo, false);
        assert.equal(notaExacta.evaluaciones.length, 3);
        assert.equal(notaInferior.riesgo, true);
        assert.ok(notaInferior.promedio < 11);

        const filtros = new URLSearchParams({ semestre: "2030-1", programa: "Programa de prueba" });
        const reporte = await peticion(`/api/reportes?${filtros}`);
        assert.equal(reporte.resumen.matriculas, 1);
        assert.equal(reporte.resumen.estudiantes, 1);
        assert.equal(reporte.resumen.promedio, 11);
        assert.equal(reporte.resumen.alertas, 0);
        assert.equal(reporte.estudiantes[0].id, exacto);

        const taller = await crear("/api/cursos", {
            tipo: "taller", asignaturaId: asignatura, profesorId: profesor,
            semestre: "2030-2", horario: "Martes", aula: "A2"
        });
        await peticion(`/api/cursos/${taller}/matriculas`, "POST", { estudianteId: exacto }, 201);
        await peticion(`/api/cursos/${taller}/notas`, "POST", {
            profesorId: profesor, estudianteId: exacto, categoria: "parcial", nota: 14
        }, 400);
        await peticion(`/api/cursos/${taller}/notas`, "POST", {
            profesorId: profesor, estudianteId: exacto, categoria: "practica", nota: 14
        }, 201);
        const estadoTaller = await peticion("/api/estado");
        assert.equal(estadoTaller.cursos.find((actual) => actual.id === taller).matriculas[0].promedio, 14);

        const lleno = await crear("/api/cursos", {
            tipo: "regular", asignaturaId: asignatura, profesorId: profesor,
            semestre: "2031-1", horario: "Viernes", aula: "A3"
        });
        const estudiantes = await Promise.all(Array.from({ length: 41 }, (_, indice) =>
            crear("/api/estudiantes", { nombre: `Estudiante ${indice}`, programa: "Sistemas" })));
        const respuestas = await Promise.all(estudiantes.map((estudianteId) =>
            fetch(`${base}/api/cursos/${lleno}/matriculas`, {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({ estudianteId })
            })));
        assert.equal(respuestas.filter((respuesta) => respuesta.status === 201).length, 40);
        assert.equal(respuestas.filter((respuesta) => respuesta.status === 409).length, 1);
        const estadoFinal = await peticion("/api/estado");
        assert.equal(estadoFinal.cursos.find((actual) => actual.id === lleno).matriculas.length, 40);

        console.log("Pruebas de API aprobadas.");
    } finally {
        if (servidor.exitCode === null && servidor.signalCode === null) {
            servidor.kill();
            await once(servidor, "close");
        }
    }
}

await ejecutar();
