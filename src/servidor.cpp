#include "modelo.hpp"

#include <crow.h>

#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>

namespace {

using Json = crow::json::wvalue;
using ListaJson = crow::json::wvalue::list;

crow::response responderJson(int estado, const Json& contenido) {
    crow::response respuesta(estado, contenido.dump());
    respuesta.set_header("Content-Type", "application/json; charset=utf-8");
    respuesta.set_header("Cache-Control", "no-store");
    respuesta.set_header("X-Content-Type-Options", "nosniff");
    return respuesta;
}

crow::response responderError(int estado, const std::string& mensaje) {
    Json contenido;
    contenido["error"] = mensaje;
    return responderJson(estado, contenido);
}

template <typename Funcion>
crow::response manejarErrores(Funcion&& funcion) {
    try {
        return funcion();
    } catch (const innova::Error& error) {
        return responderError(error.estado(), error.what());
    } catch (const std::exception& error) {
        std::cerr << "Error interno: " << error.what() << '\n';
        return responderError(500, "Ocurrió un error interno.");
    }
}

crow::json::rvalue leerObjeto(const crow::request& solicitud) {
    if (solicitud.body.size() > 16384) {
        throw innova::Error(413, "La solicitud es demasiado grande.");
    }
    auto objeto = crow::json::load(solicitud.body);
    if (!objeto || objeto.t() != crow::json::type::Object) {
        throw innova::Error(400, "Se esperaba un objeto JSON válido.");
    }
    return objeto;
}

std::string campoTexto(const crow::json::rvalue& objeto, const char* nombre) {
    if (!objeto.has(nombre) || objeto[nombre].t() != crow::json::type::String) {
        throw innova::Error(400, "El campo " + std::string(nombre) + " debe ser texto.");
    }
    return std::string(objeto[nombre].s());
}

double campoNumero(const crow::json::rvalue& objeto, const char* nombre) {
    if (!objeto.has(nombre) || objeto[nombre].t() != crow::json::type::Number) {
        throw innova::Error(400, "El campo " + std::string(nombre) + " debe ser numérico.");
    }
    const double valor = objeto[nombre].d();
    if (!std::isfinite(valor)) {
        throw innova::Error(400, "El campo " + std::string(nombre) + " debe ser finito.");
    }
    return valor;
}

int campoId(const crow::json::rvalue& objeto, const char* nombre) {
    const double valor = campoNumero(objeto, nombre);
    if (valor < 1 || valor > static_cast<double>(std::numeric_limits<int>::max()) ||
        std::floor(valor) != valor) {
        throw innova::Error(400, "El campo " + std::string(nombre) + " debe ser un ID positivo.");
    }
    return static_cast<int>(valor);
}

std::string parametro(const crow::request& solicitud, const char* nombre) {
    const char* valor = solicitud.url_params.get(nombre);
    if (valor == nullptr) {
        return "";
    }
    const std::string texto(valor);
    if (texto.size() > 120) {
        throw innova::Error(400, "El filtro " + std::string(nombre) + " es demasiado largo.");
    }
    return texto;
}

void ponerPromedio(Json& objeto, const std::optional<double>& promedio) {
    if (promedio) {
        objeto["promedio"] = *promedio;
    } else {
        objeto["promedio"] = nullptr;
    }
}

Json serializarEstado(const innova::Instituto& instituto) {
    Json resultado;
    ListaJson estudiantes;
    ListaJson profesores;
    ListaJson asignaturas;
    ListaJson cursos;

    for (const auto& entrada : instituto.estudiantes()) {
        const auto& estudiante = entrada.second;
        Json fila;
        fila["id"] = estudiante.id();
        fila["nombre"] = estudiante.nombre();
        fila["programa"] = estudiante.programa();
        estudiantes.push_back(std::move(fila));
    }
    for (const auto& entrada : instituto.profesores()) {
        const auto& profesor = entrada.second;
        Json fila;
        fila["id"] = profesor.id();
        fila["nombre"] = profesor.nombre();
        fila["especialidad"] = profesor.especialidad();
        profesores.push_back(std::move(fila));
    }
    for (const auto& entrada : instituto.asignaturas()) {
        const auto& asignatura = entrada.second;
        Json fila;
        fila["id"] = asignatura.id();
        fila["nombre"] = asignatura.nombre();
        asignaturas.push_back(std::move(fila));
    }
    for (const innova::Curso* curso : instituto.cursos()) {
        Json fila;
        fila["id"] = curso->id();
        fila["tipo"] = curso->tipo();
        fila["asignaturaId"] = curso->asignaturaId();
        fila["asignatura"] = instituto.asignatura(curso->asignaturaId()).nombre();
        fila["profesorId"] = curso->profesorId();
        fila["profesor"] = instituto.profesor(curso->profesorId()).nombre();
        fila["semestre"] = curso->semestre();
        fila["horario"] = curso->horario();
        fila["aula"] = curso->aula();
        fila["capacidad"] = static_cast<unsigned long long>(innova::Curso::Capacidad);

        ListaJson matriculas;
        for (const auto& matricula : curso->matriculas()) {
            const auto& estudiante = instituto.estudiante(matricula.estudianteId());
            const auto promedio = estudiante.consultarPromedio(*curso);
            Json registro;
            registro["estudianteId"] = estudiante.id();
            registro["estudiante"] = estudiante.nombre();
            registro["programa"] = estudiante.programa();
            ponerPromedio(registro, promedio);
            registro["riesgo"] = innova::estaEnRiesgo(promedio);

            ListaJson evaluaciones;
            for (const auto& evaluacion : matricula.evaluaciones()) {
                Json nota;
                nota["categoria"] = innova::textoTipoEvaluacion(evaluacion.tipo);
                nota["nota"] = evaluacion.nota;
                evaluaciones.push_back(std::move(nota));
            }
            registro["evaluaciones"] = std::move(evaluaciones);
            matriculas.push_back(std::move(registro));
        }
        fila["matriculas"] = std::move(matriculas);
        cursos.push_back(std::move(fila));
    }

    resultado["estudiantes"] = std::move(estudiantes);
    resultado["profesores"] = std::move(profesores);
    resultado["asignaturas"] = std::move(asignaturas);
    resultado["cursos"] = std::move(cursos);
    return resultado;
}

Json serializarReporte(const innova::Instituto& instituto, const innova::Reporte& reporte) {
    Json resultado;
    Json resumen;
    resumen["matriculas"] = static_cast<unsigned long long>(reporte.matriculas);
    resumen["estudiantes"] = static_cast<unsigned long long>(reporte.estudiantes);
    resumen["cursos"] = static_cast<unsigned long long>(reporte.cursos);
    resumen["completos"] = static_cast<unsigned long long>(reporte.completos);
    resumen["pendientes"] = static_cast<unsigned long long>(reporte.pendientes);
    resumen["alertas"] = static_cast<unsigned long long>(reporte.alertas);
    ponerPromedio(resumen, reporte.promedioGeneral);
    resultado["resumen"] = std::move(resumen);

    ListaJson cursos;
    for (const auto& actual : reporte.porCurso) {
        const auto& curso = instituto.curso(actual.cursoId);
        Json fila;
        fila["id"] = curso.id();
        fila["asignatura"] = instituto.asignatura(curso.asignaturaId()).nombre();
        fila["profesor"] = instituto.profesor(curso.profesorId()).nombre();
        fila["tipo"] = curso.tipo();
        fila["semestre"] = curso.semestre();
        fila["matriculas"] = static_cast<unsigned long long>(actual.matriculas);
        fila["completos"] = static_cast<unsigned long long>(actual.completos);
        fila["alertas"] = static_cast<unsigned long long>(actual.alertas);
        ponerPromedio(fila, actual.promedio);
        cursos.push_back(std::move(fila));
    }
    resultado["cursos"] = std::move(cursos);

    ListaJson estudiantes;
    for (const auto& actual : reporte.porEstudiante) {
        const auto& estudiante = instituto.estudiante(actual.estudianteId);
        Json fila;
        fila["id"] = estudiante.id();
        fila["nombre"] = estudiante.nombre();
        fila["programa"] = estudiante.programa();
        fila["cursos"] = static_cast<unsigned long long>(actual.cursos);
        fila["completos"] = static_cast<unsigned long long>(actual.completos);
        fila["alertas"] = static_cast<unsigned long long>(actual.alertas);
        ponerPromedio(fila, actual.promedio);
        estudiantes.push_back(std::move(fila));
    }
    resultado["estudiantes"] = std::move(estudiantes);
    return resultado;
}

crow::response creado(int id) {
    Json resultado;
    resultado["id"] = id;
    return responderJson(201, resultado);
}

crow::response creado(const std::string& mensaje) {
    Json resultado;
    resultado["mensaje"] = mensaje;
    return responderJson(201, resultado);
}

crow::response servirArchivo(const std::string& ruta, const std::string& tipo) {
    std::ifstream archivo(ruta, std::ios::binary);
    if (!archivo) {
        std::cerr << "No se pudo abrir el archivo web: " << ruta << '\n';
        return responderError(500, "No se pudo cargar la interfaz web.");
    }
    std::ostringstream contenido;
    contenido << archivo.rdbuf();
    if (archivo.bad()) {
        std::cerr << "No se pudo leer el archivo web: " << ruta << '\n';
        return responderError(500, "No se pudo cargar la interfaz web.");
    }
    crow::response respuesta(200, contenido.str());
    respuesta.set_header("Content-Type", tipo);
    respuesta.set_header("X-Content-Type-Options", "nosniff");
    return respuesta;
}

std::optional<int> puertoConfigurado() {
    std::string texto;
#ifdef _MSC_VER
    char valor[32] = {};
    std::size_t longitud = 0;
    if (getenv_s(&longitud, valor, sizeof(valor), "PORT") != 0) {
        std::cerr << "No se pudo leer la variable PORT.\n";
        return std::nullopt;
    }
    if (longitud == 0) {
        return 8080;
    }
    texto = valor;
#else
    const char* configurado = std::getenv("PORT");
    if (configurado == nullptr) {
        return 8080;
    }
    texto = configurado;
#endif
    int puerto = 0;
    const auto resultado = std::from_chars(texto.data(), texto.data() + texto.size(), puerto);
    if (resultado.ec != std::errc{} || resultado.ptr != texto.data() + texto.size() ||
        puerto < 1 || puerto > 65535) {
        std::cerr << "La variable PORT debe contener un puerto entre 1 y 65535.\n";
        return std::nullopt;
    }
    return puerto;
}

}  // namespace

int main() {
    const auto puerto = puertoConfigurado();
    if (!puerto) {
        return 1;
    }

    innova::Instituto instituto = innova::crearDatosDemostracion();
    std::mutex mutexInstituto;
    crow::SimpleApp app;

    CROW_ROUTE(app, "/")([] {
        return servirArchivo("web/index.html", "text/html; charset=utf-8");
    });
    CROW_ROUTE(app, "/styles.css")([] {
        return servirArchivo("web/styles.css", "text/css; charset=utf-8");
    });
    CROW_ROUTE(app, "/app.js")([] {
        return servirArchivo("web/app.js", "text/javascript; charset=utf-8");
    });
    CROW_ROUTE(app, "/health")([] {
        Json resultado;
        resultado["estado"] = "ok";
        return responderJson(200, resultado);
    });

    CROW_ROUTE(app, "/api/estado")([&] {
        return manejarErrores([&] {
            std::lock_guard<std::mutex> bloqueo(mutexInstituto);
            return responderJson(200, serializarEstado(instituto));
        });
    });

    CROW_ROUTE(app, "/api/reportes")([&](const crow::request& solicitud) {
        return manejarErrores([&] {
            const auto semestre = parametro(solicitud, "semestre");
            const auto programa = parametro(solicitud, "programa");
            std::lock_guard<std::mutex> bloqueo(mutexInstituto);
            const auto reporte = innova::ServicioReportes::generar(instituto, semestre, programa);
            return responderJson(200, serializarReporte(instituto, reporte));
        });
    });

    CROW_ROUTE(app, "/api/estudiantes").methods(crow::HTTPMethod::POST)
    ([&](const crow::request& solicitud) {
        return manejarErrores([&] {
            const auto datos = leerObjeto(solicitud);
            const auto nombre = campoTexto(datos, "nombre");
            const auto programa = campoTexto(datos, "programa");
            std::lock_guard<std::mutex> bloqueo(mutexInstituto);
            return creado(instituto.registrarEstudiante(nombre, programa));
        });
    });

    CROW_ROUTE(app, "/api/profesores").methods(crow::HTTPMethod::POST)
    ([&](const crow::request& solicitud) {
        return manejarErrores([&] {
            const auto datos = leerObjeto(solicitud);
            const auto nombre = campoTexto(datos, "nombre");
            const auto especialidad = campoTexto(datos, "especialidad");
            std::lock_guard<std::mutex> bloqueo(mutexInstituto);
            return creado(instituto.registrarProfesor(nombre, especialidad));
        });
    });

    CROW_ROUTE(app, "/api/asignaturas").methods(crow::HTTPMethod::POST)
    ([&](const crow::request& solicitud) {
        return manejarErrores([&] {
            const auto datos = leerObjeto(solicitud);
            const auto nombre = campoTexto(datos, "nombre");
            std::lock_guard<std::mutex> bloqueo(mutexInstituto);
            return creado(instituto.registrarAsignatura(nombre));
        });
    });

    CROW_ROUTE(app, "/api/cursos").methods(crow::HTTPMethod::POST)
    ([&](const crow::request& solicitud) {
        return manejarErrores([&] {
            const auto datos = leerObjeto(solicitud);
            const auto tipo = campoTexto(datos, "tipo");
            const auto asignaturaId = campoId(datos, "asignaturaId");
            const auto profesorId = campoId(datos, "profesorId");
            const auto semestre = campoTexto(datos, "semestre");
            const auto horario = campoTexto(datos, "horario");
            const auto aula = campoTexto(datos, "aula");
            std::lock_guard<std::mutex> bloqueo(mutexInstituto);
            return creado(instituto.crearCurso(tipo, asignaturaId, profesorId, semestre,
                                               horario, aula));
        });
    });

    CROW_ROUTE(app, "/api/cursos/<int>/matriculas").methods(crow::HTTPMethod::POST)
    ([&](const crow::request& solicitud, int cursoId) {
        return manejarErrores([&] {
            const auto datos = leerObjeto(solicitud);
            const auto estudianteId = campoId(datos, "estudianteId");
            std::lock_guard<std::mutex> bloqueo(mutexInstituto);
            instituto.matricular(cursoId, estudianteId);
            return creado("Estudiante matriculado.");
        });
    });

    CROW_ROUTE(app, "/api/cursos/<int>/notas").methods(crow::HTTPMethod::POST)
    ([&](const crow::request& solicitud, int cursoId) {
        return manejarErrores([&] {
            const auto datos = leerObjeto(solicitud);
            const auto profesorId = campoId(datos, "profesorId");
            const auto estudianteId = campoId(datos, "estudianteId");
            const auto categoria = innova::tipoEvaluacionDesdeTexto(campoTexto(datos, "categoria"));
            const auto nota = campoNumero(datos, "nota");
            std::lock_guard<std::mutex> bloqueo(mutexInstituto);
            instituto.registrarNota(cursoId, profesorId, estudianteId, categoria, nota);
            return creado("Nota registrada.");
        });
    });

    std::cout << "Instituto Innova disponible en el puerto " << *puerto << ".\n";
    app.loglevel(crow::LogLevel::Warning)
        .bindaddr("0.0.0.0")
        .port(static_cast<std::uint16_t>(*puerto))
        .concurrency(2)
        .run();
}
