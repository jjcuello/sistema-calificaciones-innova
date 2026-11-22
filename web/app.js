"use strict";

const estado = { datos: null, reporteGeneral: null };
const porId = (id) => document.getElementById(id);
const tipos = {
    regular: "Regular",
    taller: "Taller",
    teorico_practico: "Teórico-práctico"
};
const categorias = {
    parcial: "Parciales",
    practica: "Prácticas",
    examen_final: "Examen final"
};
const titulos = {
    inicio: "Resumen general",
    cursos: "Cursos y matrículas",
    calificaciones: "Registrar calificaciones",
    consulta: "Consulta del estudiante",
    reportes: "Reportes consolidados",
    administracion: "Administración"
};

function escapar(valor) {
    const caracteres = { "&": "&amp;", "<": "&lt;", ">": "&gt;", "\"": "&quot;", "'": "&#39;" };
    return String(valor ?? "").replace(/[&<>"']/g, (caracter) => caracteres[caracter]);
}

function mostrarMensaje(tipo, texto) {
    const mensaje = porId("message");
    mensaje.className = `message ${tipo}`;
    mensaje.textContent = texto;
    mensaje.hidden = false;
}

async function solicitar(url, opciones = {}) {
    const respuesta = await fetch(url, { cache: "no-store", ...opciones });
    let contenido;
    try {
        contenido = await respuesta.json();
    } catch (error) {
        throw new Error(`El servidor devolvió una respuesta inválida (HTTP ${respuesta.status}).`, { cause: error });
    }
    if (!respuesta.ok) {
        throw new Error(contenido.error || `La solicitud falló (HTTP ${respuesta.status}).`);
    }
    return contenido;
}

function enviar(url, datos) {
    return solicitar(url, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(datos)
    });
}

function formatoPromedio(valor, riesgo = false) {
    if (valor === null || valor === undefined) {
        return "Pendiente";
    }
    if (riesgo && valor < 11 && Math.round(valor * 100) / 100 >= 11) {
        return String(valor).replace(".", ",");
    }
    return new Intl.NumberFormat("es-VE", { minimumFractionDigits: 2, maximumFractionDigits: 2 }).format(valor);
}

function formatoNota(valor) {
    return new Intl.NumberFormat("es-VE", { maximumFractionDigits: 6 }).format(valor);
}

function distintivo(matricula) {
    if (matricula.promedio === null) {
        return '<span class="pill pill-gray">Pendiente</span>';
    }
    if (matricula.riesgo) {
        return '<span class="pill pill-red">En riesgo</span>';
    }
    return '<span class="pill pill-green">Completo</span>';
}

function filaVacia(columnas, texto) {
    return `<tr><td colspan="${columnas}" class="empty-state">${escapar(texto)}</td></tr>`;
}

function actualizarOpciones(id, opciones, vacio, elegirPrimero = false) {
    const select = porId(id);
    const anterior = select.value;
    select.replaceChildren(new Option(vacio, ""));
    for (const opcion of opciones) {
        select.add(new Option(opcion.texto, String(opcion.valor)));
    }
    if (opciones.some((opcion) => String(opcion.valor) === anterior)) {
        select.value = anterior;
    } else {
        select.value = elegirPrimero && opciones.length ? String(opciones[0].valor) : "";
    }
}

function opcionesCursos(cursos) {
    return cursos.map((curso) => ({
        valor: curso.id,
        texto: `${curso.asignatura} · ${curso.semestre} · ${curso.matriculas.length}/${curso.capacidad}`
    }));
}

function actualizarSelectores() {
    const { estudiantes, profesores, asignaturas, cursos } = estado.datos;
    const opcionesEstudiantes = estudiantes.map((estudiante) => ({
        valor: estudiante.id,
        texto: `${estudiante.nombre} · ${estudiante.programa}`
    }));
    const opcionesProfesores = profesores.map((profesor) => ({
        valor: profesor.id,
        texto: profesor.nombre
    }));
    actualizarOpciones("course-subject", asignaturas.map((asignatura) => ({
        valor: asignatura.id,
        texto: asignatura.nombre
    })), "Selecciona una asignatura");
    actualizarOpciones("course-teacher", opcionesProfesores, "Selecciona un profesor");
    actualizarOpciones("enroll-course", opcionesCursos(cursos), "Selecciona un curso");
    actualizarOpciones("enroll-student", opcionesEstudiantes, "Selecciona un estudiante");
    actualizarOpciones("grade-teacher", opcionesProfesores, "Selecciona un profesor", true);
    actualizarOpciones("student-view", opcionesEstudiantes, "Selecciona un estudiante", true);

    const semestres = [...new Set(cursos.map((curso) => curso.semestre))].sort();
    const programas = [...new Set(estudiantes.map((estudiante) => estudiante.programa))]
        .sort((primero, segundo) => primero.localeCompare(segundo, "es"));
    actualizarOpciones("report-semester", semestres.map((semestre) => ({
        valor: semestre, texto: semestre
    })), "Todos los semestres");
    actualizarOpciones("report-program", programas.map((programa) => ({
        valor: programa, texto: programa
    })), "Todos los programas");
    actualizarCursosProfesor();
}

function actualizarCursosProfesor() {
    const profesorId = Number(porId("grade-teacher").value);
    const cursos = estado.datos.cursos.filter((curso) => curso.profesorId === profesorId);
    actualizarOpciones("grade-course", opcionesCursos(cursos), "Sin cursos asignados", true);
    actualizarMatriculasProfesor();
}

function actualizarMatriculasProfesor() {
    const curso = estado.datos.cursos.find((actual) => actual.id === Number(porId("grade-course").value));
    const matriculas = curso ? curso.matriculas : [];
    actualizarOpciones("grade-student", matriculas.map((matricula) => ({
        valor: matricula.estudianteId,
        texto: matricula.estudiante
    })), "Sin estudiantes matriculados", true);
    const disponibles = curso && curso.tipo === "taller"
        ? [{ valor: "practica", texto: "Prácticas" }]
        : Object.entries(categorias).map(([valor, texto]) => ({ valor, texto }));
    actualizarOpciones("grade-category", disponibles, "Selecciona una categoría", true);
    mostrarRegistroProfesor(curso);
}

function notasPorCategoria(matricula, categoria) {
    return matricula.evaluaciones
        .filter((evaluacion) => evaluacion.categoria === categoria)
        .map((evaluacion) => formatoNota(evaluacion.nota))
        .join(", ") || "Sin registrar";
}

function mostrarRegistroProfesor(curso) {
    const contenedor = porId("grade-roster");
    if (!curso || !curso.matriculas.length) {
        contenedor.innerHTML = '<p class="empty-state">No hay estudiantes en esta sección.</p>';
        return;
    }
    contenedor.innerHTML = curso.matriculas.map((matricula) => {
        const detalle = curso.tipo === "taller"
            ? `Prácticas: ${escapar(notasPorCategoria(matricula, "practica"))}`
            : `Parciales: ${escapar(notasPorCategoria(matricula, "parcial"))} · ` +
              `Prácticas: ${escapar(notasPorCategoria(matricula, "practica"))} · ` +
              `Final: ${escapar(notasPorCategoria(matricula, "examen_final"))}`;
        return `<div class="roster-item">
            <div class="roster-top"><strong>${escapar(matricula.estudiante)}</strong>${distintivo(matricula)}</div>
            <p class="roster-detail">${detalle}<br>Promedio: <strong>${escapar(formatoPromedio(matricula.promedio, matricula.riesgo))}</strong></p>
        </div>`;
    }).join("");
}

function mostrarResumen() {
    const { estudiantes, cursos } = estado.datos;
    const resumen = estado.reporteGeneral.resumen;
    porId("metric-students").textContent = estudiantes.length;
    porId("metric-courses").textContent = cursos.length;
    porId("metric-average").textContent = formatoPromedio(resumen.promedio);
    porId("metric-alerts").textContent = resumen.alertas;

    const alertas = cursos.flatMap((curso) => curso.matriculas
        .filter((matricula) => matricula.riesgo)
        .map((matricula) => ({ curso, matricula })));
    porId("alert-list").innerHTML = alertas.length
        ? alertas.map(({ curso, matricula }) => `<div class="alert-item">
            <div><strong>${escapar(matricula.estudiante)}</strong><span>${escapar(curso.asignatura)} · ${escapar(curso.semestre)}</span></div>
            <span class="alert-score">${escapar(formatoPromedio(matricula.promedio, true))} / 20</span>
        </div>`).join("")
        : '<p class="empty-state">No hay alertas de rendimiento.</p>';
}

function mostrarCursos() {
    const filas = estado.datos.cursos.map((curso) => `<tr>
        <td><strong>${escapar(curso.asignatura)}</strong><span class="cell-detail">Sección #${curso.id} · ${escapar(curso.aula)}</span></td>
        <td><span class="pill pill-blue">${escapar(tipos[curso.tipo] || curso.tipo)}</span></td>
        <td>${escapar(curso.profesor)}</td>
        <td>${escapar(curso.semestre)}<span class="cell-detail">${escapar(curso.horario)}</span></td>
        <td><strong>${curso.matriculas.length} / ${curso.capacidad}</strong></td>
    </tr>`);
    porId("courses-body").innerHTML = filas.join("") || filaVacia(5, "Aún no hay cursos registrados.");
}

function mostrarEstudiante() {
    const id = Number(porId("student-view").value);
    const estudiante = estado.datos.estudiantes.find((actual) => actual.id === id);
    const contenedor = porId("student-results");
    if (!estudiante) {
        contenedor.innerHTML = '<p class="empty-state">Selecciona un estudiante.</p>';
        return;
    }
    const matriculas = estado.datos.cursos.flatMap((curso) => curso.matriculas
        .filter((matricula) => matricula.estudianteId === id)
        .map((matricula) => ({ curso, matricula })));
    if (!matriculas.length) {
        contenedor.innerHTML = '<p class="empty-state">Este estudiante no tiene cursos matriculados.</p>';
        return;
    }
    const filas = matriculas.map(({ curso, matricula }) => `<tr>
        <td><strong>${escapar(curso.asignatura)}</strong><span class="cell-detail">${escapar(curso.semestre)} · ${escapar(tipos[curso.tipo] || curso.tipo)}</span></td>
        <td>${curso.tipo === "taller" ? "No aplica" : escapar(notasPorCategoria(matricula, "parcial"))}</td>
        <td>${escapar(notasPorCategoria(matricula, "practica"))}</td>
        <td>${curso.tipo === "taller" ? "No aplica" : escapar(notasPorCategoria(matricula, "examen_final"))}</td>
        <td><strong>${escapar(formatoPromedio(matricula.promedio, matricula.riesgo))}</strong></td>
        <td>${distintivo(matricula)}</td>
    </tr>`).join("");
    contenedor.innerHTML = `<h3 class="student-heading">${escapar(estudiante.nombre)}</h3>
        <p class="muted">${escapar(estudiante.programa)}</p>
        <div class="table-wrap"><table>
            <caption class="sr-only">Calificaciones del estudiante seleccionado</caption>
            <thead><tr><th>Curso</th><th>Parciales</th><th>Prácticas</th><th>Final</th><th>Promedio</th><th>Estado</th></tr></thead>
            <tbody>${filas}</tbody>
        </table></div>`;
}

function mostrarReporte(reporte) {
    const resumen = reporte.resumen;
    porId("report-enrollments").textContent = resumen.matriculas;
    porId("report-students").textContent = resumen.estudiantes;
    porId("report-average").textContent = formatoPromedio(resumen.promedio);
    porId("report-alerts").textContent = resumen.alertas;
    porId("report-pending").textContent = resumen.pendientes;

    const cursos = reporte.cursos.map((curso) => `<tr>
        <td><strong>${escapar(curso.asignatura)}</strong><span class="cell-detail">${escapar(tipos[curso.tipo] || curso.tipo)} · ${escapar(curso.profesor)}</span></td>
        <td>${escapar(curso.semestre)}</td><td>${curso.matriculas}</td><td>${curso.completos}</td>
        <td>${escapar(formatoPromedio(curso.promedio))}</td><td>${curso.alertas}</td>
    </tr>`);
    const estudiantes = reporte.estudiantes.map((estudiante) => `<tr>
        <td><strong>${escapar(estudiante.nombre)}</strong></td><td>${escapar(estudiante.programa)}</td>
        <td>${estudiante.cursos}</td><td>${estudiante.completos}</td>
        <td>${escapar(formatoPromedio(estudiante.promedio))}</td><td>${estudiante.alertas}</td>
    </tr>`);
    porId("report-courses").innerHTML = cursos.join("") || filaVacia(6, "No hay cursos para estos filtros.");
    porId("report-students-body").innerHTML = estudiantes.join("") || filaVacia(6, "No hay estudiantes para estos filtros.");
}

async function actualizarReporte() {
    const filtros = new URLSearchParams();
    if (porId("report-semester").value) {
        filtros.set("semestre", porId("report-semester").value);
    }
    if (porId("report-program").value) {
        filtros.set("programa", porId("report-program").value);
    }
    const reporte = filtros.size
        ? await solicitar(`/api/reportes?${filtros}`)
        : estado.reporteGeneral;
    mostrarReporte(reporte);
}

async function refrescar() {
    const [datos, reporteGeneral] = await Promise.all([
        solicitar("/api/estado"),
        solicitar("/api/reportes")
    ]);
    estado.datos = datos;
    estado.reporteGeneral = reporteGeneral;
    actualizarSelectores();
    mostrarResumen();
    mostrarCursos();
    mostrarEstudiante();
    await actualizarReporte();
}

function enlazarFormulario(id, operacion, mensaje) {
    const formulario = porId(id);
    formulario.addEventListener("submit", async (evento) => {
        evento.preventDefault();
        const boton = formulario.querySelector('button[type="submit"]');
        boton.disabled = true;
        let guardado = false;
        try {
            const datos = Object.fromEntries(new FormData(formulario));
            await operacion(datos);
            guardado = true;
            if (id === "grade-form") {
                formulario.querySelector('[name="nota"]').value = "";
            } else {
                formulario.reset();
            }
            await refrescar();
            mostrarMensaje("success", mensaje);
        } catch (error) {
            console.error(error);
            mostrarMensaje("error", guardado
                ? `La operación se guardó, pero no se pudo actualizar la vista: ${error.message}`
                : error.message);
        } finally {
            boton.disabled = false;
        }
    });
}

function mostrarVista(vista) {
    for (const pagina of document.querySelectorAll(".page")) {
        pagina.hidden = pagina.id !== `vista-${vista}`;
    }
    for (const boton of document.querySelectorAll(".nav-link")) {
        const activa = boton.dataset.view === vista;
        boton.classList.toggle("active", activa);
        if (activa) {
            boton.setAttribute("aria-current", "page");
        } else {
            boton.removeAttribute("aria-current");
        }
    }
    porId("page-title").textContent = titulos[vista];
    porId("message").hidden = true;
}

for (const boton of document.querySelectorAll(".nav-link")) {
    boton.addEventListener("click", () => mostrarVista(boton.dataset.view));
}
porId("grade-teacher").addEventListener("change", actualizarCursosProfesor);
porId("grade-course").addEventListener("change", actualizarMatriculasProfesor);
porId("student-view").addEventListener("change", mostrarEstudiante);
porId("report-form").addEventListener("submit", async (evento) => {
    evento.preventDefault();
    try {
        await actualizarReporte();
    } catch (error) {
        console.error(error);
        mostrarMensaje("error", error.message);
    }
});
porId("refresh-button").addEventListener("click", async () => {
    try {
        await refrescar();
        mostrarMensaje("success", "Los datos se han actualizado.");
    } catch (error) {
        console.error(error);
        mostrarMensaje("error", error.message);
    }
});

enlazarFormulario("student-form", (datos) => enviar("/api/estudiantes", datos), "Estudiante registrado.");
enlazarFormulario("teacher-form", (datos) => enviar("/api/profesores", datos), "Profesor registrado.");
enlazarFormulario("subject-form", (datos) => enviar("/api/asignaturas", datos), "Asignatura registrada.");
enlazarFormulario("course-form", (datos) => enviar("/api/cursos", {
    ...datos, asignaturaId: Number(datos.asignaturaId), profesorId: Number(datos.profesorId)
}), "Curso creado.");
enlazarFormulario("enroll-form", (datos) => enviar(`/api/cursos/${Number(datos.cursoId)}/matriculas`, {
    estudianteId: Number(datos.estudianteId)
}), "Matrícula registrada.");
enlazarFormulario("grade-form", (datos) => enviar(`/api/cursos/${Number(datos.cursoId)}/notas`, {
    profesorId: Number(datos.profesorId), estudianteId: Number(datos.estudianteId),
    categoria: datos.categoria, nota: Number(datos.nota.replace(",", "."))
}), "Nota registrada.");

refrescar().catch((error) => {
    console.error(error);
    mostrarMensaje("error", `No se pudieron cargar los datos: ${error.message}`);
});
