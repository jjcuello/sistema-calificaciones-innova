#include "modelo.hpp"

#include <cmath>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <string>

namespace {

void comprobar(bool condicion, const std::string& mensaje) {
    if (!condicion) {
        std::cerr << "FALLO: " << mensaje << '\n';
        std::exit(1);
    }
}

void comprobarPromedio(const std::optional<double>& valor, double esperado,
                       const std::string& mensaje) {
    comprobar(valor.has_value() && std::abs(*valor - esperado) < 1e-9, mensaje);
}

void comprobarError(int estado, const std::function<void()>& operacion,
                    const std::string& mensaje) {
    try {
        operacion();
    } catch (const innova::Error& error) {
        comprobar(error.estado() == estado, mensaje + ": estado inesperado");
        return;
    }
    comprobar(false, mensaje + ": no se rechazó la operación");
}

void probarCalificaciones() {
    innova::Instituto instituto;
    const int estudiante = instituto.registrarEstudiante("Ana", "Sistemas");
    const int profesor = instituto.registrarProfesor("María", "Programación");
    const int otroProfesor = instituto.registrarProfesor("Carlos", "Laboratorio");
    const int asignatura = instituto.registrarAsignatura("Programación");
    const int regular = instituto.crearCurso("regular", asignatura, profesor, "2026-2",
                                              "Lunes", "A1");
    const int taller = instituto.crearCurso("taller", asignatura, profesor, "2026-2",
                                             "Martes", "A2");
    const int mixto = instituto.crearCurso("teorico_practico", asignatura, profesor,
                                           "2026-2", "Viernes", "A3");
    for (const int curso : {regular, taller, mixto}) {
        instituto.matricular(curso, estudiante);
        comprobar(!instituto.curso(curso).calcularPromedio(estudiante),
                  "Una evaluación incompleta debe estar pendiente.");
    }

    instituto.registrarNota(regular, profesor, estudiante, innova::TipoEvaluacion::Parcial, 10);
    instituto.registrarNota(regular, profesor, estudiante, innova::TipoEvaluacion::Parcial, 14);
    instituto.registrarNota(regular, profesor, estudiante, innova::TipoEvaluacion::Practica, 14);
    instituto.registrarNota(regular, profesor, estudiante, innova::TipoEvaluacion::ExamenFinal, 16);
    comprobarPromedio(instituto.estudiante(estudiante).consultarPromedio(instituto.curso(regular)),
                      14.6, "El curso regular debe ponderar las medias por categoría.");
    comprobar(instituto.estudiante(estudiante).consultarNotas(instituto.curso(regular)).size() == 4,
              "El estudiante debe consultar sus evaluaciones.");

    instituto.registrarNota(taller, profesor, estudiante, innova::TipoEvaluacion::Practica, 10);
    instituto.registrarNota(taller, profesor, estudiante, innova::TipoEvaluacion::Practica, 12);
    const auto promedioTaller = instituto.curso(taller).calcularPromedio(estudiante);
    comprobarPromedio(promedioTaller, 11.0, "El taller debe usar solo prácticas.");
    comprobar(!innova::estaEnRiesgo(promedioTaller), "Un promedio de 11 no está en riesgo.");
    comprobarError(400, [&] {
        instituto.registrarNota(taller, profesor, estudiante, innova::TipoEvaluacion::Parcial, 15);
    }, "Un taller no debe aceptar parciales.");

    instituto.registrarNota(mixto, profesor, estudiante, innova::TipoEvaluacion::Parcial, 10.9);
    instituto.registrarNota(mixto, profesor, estudiante, innova::TipoEvaluacion::Practica, 10.9);
    instituto.registrarNota(mixto, profesor, estudiante, innova::TipoEvaluacion::ExamenFinal, 10.9);
    const auto promedioMixto = instituto.curso(mixto).calcularPromedio(estudiante);
    comprobarPromedio(promedioMixto, 10.9, "El curso teórico-práctico usa la regla general.");
    comprobar(innova::estaEnRiesgo(promedioMixto), "Un promedio inferior a 11 debe alertar.");

    const auto cantidadAntes = instituto.curso(regular).consultarNotas(estudiante).size();
    comprobarError(403, [&] {
        instituto.registrarNota(regular, otroProfesor, estudiante, innova::TipoEvaluacion::Parcial, 10);
    }, "Un profesor no puede registrar notas en un curso ajeno.");
    for (const double invalida : {-0.1, 20.1, 10.1234567,
                                  std::numeric_limits<double>::quiet_NaN()}) {
        comprobarError(400, [&] {
            instituto.registrarNota(regular, profesor, estudiante,
                                   innova::TipoEvaluacion::Parcial, invalida);
        }, "Una nota fuera de la escala debe rechazarse.");
    }
    comprobar(instituto.curso(regular).consultarNotas(estudiante).size() == cantidadAntes,
              "Una operación rechazada no debe modificar las notas.");
}

void probarLimiteDeRiesgo() {
    innova::Instituto instituto;
    const int profesor = instituto.registrarProfesor("Docente", "Sistemas");
    const int asignatura = instituto.registrarAsignatura("Sistemas");
    const int curso = instituto.crearCurso("regular", asignatura, profesor, "2026-2",
                                           "Lunes", "A1");
    const int exacto = instituto.registrarEstudiante("Exacto", "Sistemas");
    const int inferior = instituto.registrarEstudiante("Inferior", "Sistemas");
    instituto.matricular(curso, exacto);
    instituto.matricular(curso, inferior);
    for (const int estudiante : {exacto, inferior}) {
        const double parcial = estudiante == exacto ? 1.7 : 1.699999;
        instituto.registrarNota(curso, profesor, estudiante, innova::TipoEvaluacion::Parcial,
                               parcial);
        instituto.registrarNota(curso, profesor, estudiante, innova::TipoEvaluacion::Practica, 8.2);
        instituto.registrarNota(curso, profesor, estudiante, innova::TipoEvaluacion::ExamenFinal, 16.4);
    }
    const auto promedioExacto = instituto.curso(curso).calcularPromedio(exacto);
    comprobar(promedioExacto && *promedioExacto == 11.0 && !innova::estaEnRiesgo(promedioExacto),
              "Un promedio matemáticamente igual a 11 no debe generar una falsa alerta.");
    comprobar(innova::estaEnRiesgo(instituto.curso(curso).calcularPromedio(inferior)),
              "Un promedio inferior a 11 no debe ocultarse por redondeo.");
}

void probarLimiteDeEvaluaciones() {
    innova::Instituto instituto;
    const int estudiante = instituto.registrarEstudiante("Ana", "Sistemas");
    const int profesor = instituto.registrarProfesor("Docente", "Sistemas");
    const int asignatura = instituto.registrarAsignatura("Taller");
    const int curso = instituto.crearCurso("taller", asignatura, profesor, "2026-2",
                                           "Lunes", "A1");
    instituto.matricular(curso, estudiante);
    for (std::size_t numero = 0; numero < innova::Curso::MaxEvaluacionesPorCategoria; ++numero) {
        instituto.registrarNota(curso, profesor, estudiante, innova::TipoEvaluacion::Practica, 14);
    }
    comprobarError(409, [&] {
        instituto.registrarNota(curso, profesor, estudiante, innova::TipoEvaluacion::Practica, 15);
    }, "El límite de evaluaciones debe rechazarse explícitamente.");
    comprobarPromedio(instituto.curso(curso).calcularPromedio(estudiante), 14,
                      "El rechazo no debe modificar el promedio.");
}

void probarMatriculas() {
    innova::Instituto instituto;
    const int profesor = instituto.registrarProfesor("Docente", "Sistemas");
    const int asignatura = instituto.registrarAsignatura("Sistemas");
    const int curso = instituto.crearCurso("regular", asignatura, profesor, "2026-2",
                                           "Lunes", "A1");
    int primero = 0;
    int ultimo = 0;
    for (int numero = 0; numero < 41; ++numero) {
        ultimo = instituto.registrarEstudiante("Estudiante " + std::to_string(numero), "Sistemas");
        if (numero == 0) {
            primero = ultimo;
        }
        if (numero < 40) {
            instituto.matricular(curso, ultimo);
        }
    }
    comprobar(instituto.curso(curso).matriculas().size() == 40, "El cupo 40 debe permitirse.");
    comprobarError(409, [&] { instituto.matricular(curso, ultimo); },
                   "El cupo 41 debe rechazarse.");
    comprobarError(409, [&] { instituto.matricular(curso, primero); },
                   "Una matrícula duplicada debe rechazarse.");
    comprobar(instituto.curso(curso).matriculas().size() == 40,
              "Los rechazos no deben modificar el cupo.");

    const int otroCurso = instituto.crearCurso("taller", asignatura, profesor, "2026-2",
                                               "Viernes", "A2");
    instituto.matricular(otroCurso, primero);
    comprobar(instituto.curso(otroCurso).matriculas().size() == 1,
              "Un estudiante puede matricularse en varios cursos.");
}

void probarReportes() {
    innova::Instituto instituto;
    const int ana = instituto.registrarEstudiante("Ana", "Sistemas");
    const int luis = instituto.registrarEstudiante("Luis", "Administración");
    const int profesor = instituto.registrarProfesor("Docente", "Laboratorio");
    const int asignatura = instituto.registrarAsignatura("Taller");
    const int actual = instituto.crearCurso("taller", asignatura, profesor, "2026-2",
                                            "Lunes", "A1");
    const int anterior = instituto.crearCurso("taller", asignatura, profesor, "2026-1",
                                              "Martes", "A2");
    instituto.matricular(actual, ana);
    instituto.matricular(actual, luis);
    instituto.matricular(anterior, ana);
    instituto.registrarNota(actual, profesor, ana, innova::TipoEvaluacion::Practica, 10.9);
    instituto.registrarNota(anterior, profesor, ana, innova::TipoEvaluacion::Practica, 18);

    const auto filtrado = innova::ServicioReportes::generar(instituto, "2026-2", "Sistemas");
    comprobar(filtrado.cursos == 1 && filtrado.estudiantes == 1 && filtrado.matriculas == 1,
              "El reporte debe respetar semestre y programa.");
    comprobar(filtrado.alertas == 1 && filtrado.pendientes == 0,
              "El reporte debe contabilizar la alerta filtrada.");
    comprobarPromedio(filtrado.promedioGeneral, 10.9, "El promedio filtrado debe ser correcto.");

    const auto semestre = innova::ServicioReportes::generar(instituto, "2026-2");
    comprobar(semestre.matriculas == 2 && semestre.completos == 1 && semestre.pendientes == 1,
              "Los promedios pendientes no deben contarse como cero.");
    comprobarPromedio(semestre.promedioGeneral, 10.9,
                      "El promedio consolidado solo debe incluir resultados completos.");
    comprobar(!innova::ServicioReportes::generar(instituto, "2030-1").promedioGeneral,
              "Un reporte sin resultados completos no debe inventar un promedio.");
}

}  // namespace

int main() {
    probarCalificaciones();
    probarLimiteDeRiesgo();
    probarLimiteDeEvaluaciones();
    probarMatriculas();
    probarReportes();
    std::cout << "Pruebas del modelo aprobadas.\n";
}
