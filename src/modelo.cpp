#include "modelo.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace innova {
namespace {

constexpr std::int64_t EscalaNota = 1000000;

struct AcumuladoNotas {
    std::int64_t suma = 0;
    std::int64_t cantidad = 0;
};

AcumuladoNotas acumularNotas(const Matricula& matricula, TipoEvaluacion tipo) {
    AcumuladoNotas resultado;
    for (const auto& evaluacion : matricula.evaluaciones()) {
        if (evaluacion.tipo == tipo) {
            resultado.suma += static_cast<std::int64_t>(
                std::llround(evaluacion.nota * static_cast<double>(EscalaNota)));
            ++resultado.cantidad;
        }
    }
    return resultado;
}

std::string textoObligatorio(std::string texto, const std::string& campo) {
    const auto primero = texto.find_first_not_of(" \t\r\n");
    const auto ultimo = texto.find_last_not_of(" \t\r\n");
    if (primero == std::string::npos) {
        throw Error(400, "El campo " + campo + " es obligatorio.");
    }
    texto = texto.substr(primero, ultimo - primero + 1);
    if (texto.size() > 120) {
        throw Error(400, "El campo " + campo + " no puede superar 120 bytes.");
    }
    return texto;
}

void validarId(int id) {
    if (id <= 0) {
        throw Error(400, "El identificador debe ser positivo.");
    }
}

template <typename T>
const T& buscar(const std::map<int, T>& elementos, int id, const std::string& nombre) {
    const auto encontrado = elementos.find(id);
    if (encontrado == elementos.end()) {
        throw Error(404, nombre + " no encontrado.");
    }
    return encontrado->second;
}

}  // namespace

Error::Error(int estado, const std::string& mensaje)
    : std::runtime_error(mensaje), estado_(estado) {}

int Error::estado() const noexcept {
    return estado_;
}

TipoEvaluacion tipoEvaluacionDesdeTexto(const std::string& texto) {
    if (texto == "parcial") {
        return TipoEvaluacion::Parcial;
    }
    if (texto == "practica") {
        return TipoEvaluacion::Practica;
    }
    if (texto == "examen_final") {
        return TipoEvaluacion::ExamenFinal;
    }
    throw Error(400, "La categoría de evaluación no es válida.");
}

std::string textoTipoEvaluacion(TipoEvaluacion tipo) {
    switch (tipo) {
        case TipoEvaluacion::Parcial:
            return "parcial";
        case TipoEvaluacion::Practica:
            return "practica";
        case TipoEvaluacion::ExamenFinal:
            return "examen_final";
    }
    throw Error(400, "La categoría de evaluación no es válida.");
}

bool estaEnRiesgo(const std::optional<double>& promedio) noexcept {
    return promedio.has_value() && *promedio < 11.0;
}

Estudiante::Estudiante(int id, std::string nombre, std::string programa)
    : id_(id),
      nombre_(textoObligatorio(std::move(nombre), "nombre")),
      programa_(textoObligatorio(std::move(programa), "programa")) {
    validarId(id_);
}

int Estudiante::id() const noexcept {
    return id_;
}

const std::string& Estudiante::nombre() const noexcept {
    return nombre_;
}

const std::string& Estudiante::programa() const noexcept {
    return programa_;
}

std::vector<Evaluacion> Estudiante::consultarNotas(const Curso& curso) const {
    return curso.consultarNotas(id_);
}

std::optional<double> Estudiante::consultarPromedio(const Curso& curso) const {
    return curso.calcularPromedio(id_);
}

Profesor::Profesor(int id, std::string nombre, std::string especialidad)
    : id_(id),
      nombre_(textoObligatorio(std::move(nombre), "nombre")),
      especialidad_(textoObligatorio(std::move(especialidad), "especialidad")) {
    validarId(id_);
}

int Profesor::id() const noexcept {
    return id_;
}

const std::string& Profesor::nombre() const noexcept {
    return nombre_;
}

const std::string& Profesor::especialidad() const noexcept {
    return especialidad_;
}

bool Profesor::estaAsignado(const Curso& curso) const noexcept {
    return curso.profesorId() == id_;
}

void Profesor::registrarNota(Curso& curso, int estudianteId, TipoEvaluacion tipo,
                            double nota) const {
    curso.registrarNota(id_, estudianteId, tipo, nota);
}

Asignatura::Asignatura(int id, std::string nombre)
    : id_(id), nombre_(textoObligatorio(std::move(nombre), "nombre")) {
    validarId(id_);
}

int Asignatura::id() const noexcept {
    return id_;
}

const std::string& Asignatura::nombre() const noexcept {
    return nombre_;
}

Matricula::Matricula(int estudianteId) : estudianteId_(estudianteId) {}

int Matricula::estudianteId() const noexcept {
    return estudianteId_;
}

const std::vector<Evaluacion>& Matricula::evaluaciones() const noexcept {
    return evaluaciones_;
}

void Matricula::agregarEvaluacion(TipoEvaluacion tipo, double nota) {
    evaluaciones_.push_back({tipo, nota});
}

Curso::Curso(int id, int asignaturaId, int profesorId, std::string semestre,
             std::string horario, std::string aula)
    : id_(id),
      asignaturaId_(asignaturaId),
      profesorId_(profesorId),
      semestre_(textoObligatorio(std::move(semestre), "semestre")),
      horario_(textoObligatorio(std::move(horario), "horario")),
      aula_(textoObligatorio(std::move(aula), "aula")) {
    validarId(id_);
    validarId(asignaturaId_);
    validarId(profesorId_);
}

int Curso::id() const noexcept {
    return id_;
}

int Curso::asignaturaId() const noexcept {
    return asignaturaId_;
}

int Curso::profesorId() const noexcept {
    return profesorId_;
}

const std::string& Curso::semestre() const noexcept {
    return semestre_;
}

const std::string& Curso::horario() const noexcept {
    return horario_;
}

const std::string& Curso::aula() const noexcept {
    return aula_;
}

const std::vector<Matricula>& Curso::matriculas() const noexcept {
    return matriculas_;
}

void Curso::matricular(int estudianteId) {
    validarId(estudianteId);
    const auto existente = std::find_if(
        matriculas_.begin(), matriculas_.end(),
        [estudianteId](const Matricula& matricula) {
            return matricula.estudianteId() == estudianteId;
        });
    if (existente != matriculas_.end()) {
        throw Error(409, "El estudiante ya está matriculado en este curso.");
    }
    if (matriculas_.size() >= Capacidad) {
        throw Error(409, "El curso ya tiene 40 estudiantes matriculados.");
    }
    matriculas_.push_back(Matricula(estudianteId));
}

void Curso::registrarNota(int profesorId, int estudianteId, TipoEvaluacion tipo,
                          double nota) {
    if (profesorId != profesorId_) {
        throw Error(403, "El profesor no está asignado a este curso.");
    }
    if (!aceptaTipo(tipo)) {
        throw Error(400, "La categoría de evaluación no corresponde a este curso.");
    }
    if (!std::isfinite(nota) || nota < 0.0 || nota > 20.0) {
        throw Error(400, "La nota debe estar entre 0 y 20.");
    }
    const double unidades = nota * static_cast<double>(EscalaNota);
    if (std::abs(unidades - std::round(unidades)) > 0.000001) {
        throw Error(400, "La nota admite como máximo seis decimales.");
    }
    const auto matricula = std::find_if(
        matriculas_.begin(), matriculas_.end(),
        [estudianteId](const Matricula& actual) {
            return actual.estudianteId() == estudianteId;
        });
    if (matricula == matriculas_.end()) {
        throw Error(404, "El estudiante no está matriculado en este curso.");
    }
    const auto cantidad = std::count_if(
        matricula->evaluaciones().begin(), matricula->evaluaciones().end(),
        [tipo](const Evaluacion& evaluacion) { return evaluacion.tipo == tipo; });
    if (static_cast<std::size_t>(cantidad) >= MaxEvaluacionesPorCategoria) {
        throw Error(409, "Se alcanzó el límite de 100 evaluaciones en esta categoría.");
    }
    matricula->agregarEvaluacion(tipo, nota);
}

const std::vector<Evaluacion>& Curso::consultarNotas(int estudianteId) const {
    return buscarMatricula(estudianteId).evaluaciones();
}

const Matricula& Curso::buscarMatricula(int estudianteId) const {
    const auto encontrado = std::find_if(
        matriculas_.begin(), matriculas_.end(),
        [estudianteId](const Matricula& matricula) {
            return matricula.estudianteId() == estudianteId;
        });
    if (encontrado == matriculas_.end()) {
        throw Error(404, "El estudiante no está matriculado en este curso.");
    }
    return *encontrado;
}

std::optional<double> Curso::promedioCategoria(const Matricula& matricula,
                                               TipoEvaluacion tipo) {
    const auto notas = acumularNotas(matricula, tipo);
    if (notas.cantidad == 0) {
        return std::nullopt;
    }
    return static_cast<double>(notas.suma) /
           static_cast<double>(EscalaNota * notas.cantidad);
}

std::optional<double> Curso::promedioPonderado(int estudianteId) const {
    const auto& matricula = buscarMatricula(estudianteId);
    const auto parciales = acumularNotas(matricula, TipoEvaluacion::Parcial);
    const auto practicas = acumularNotas(matricula, TipoEvaluacion::Practica);
    const auto examen = acumularNotas(matricula, TipoEvaluacion::ExamenFinal);
    if (parciales.cantidad == 0 || practicas.cantidad == 0 || examen.cantidad == 0) {
        return std::nullopt;
    }
    // Los enteros conservan el umbral exacto antes de convertir el resultado.
    const std::int64_t numerador =
        20 * parciales.suma * practicas.cantidad * examen.cantidad +
        30 * practicas.suma * parciales.cantidad * examen.cantidad +
        50 * examen.suma * parciales.cantidad * practicas.cantidad;
    const std::int64_t denominador =
        100 * EscalaNota * parciales.cantidad * practicas.cantidad * examen.cantidad;
    return static_cast<double>(numerador) / static_cast<double>(denominador);
}

bool Curso::aceptaTipo(TipoEvaluacion tipo) const noexcept {
    return tipo == TipoEvaluacion::Parcial || tipo == TipoEvaluacion::Practica ||
           tipo == TipoEvaluacion::ExamenFinal;
}

std::string CursoRegular::tipo() const {
    return "regular";
}

std::optional<double> CursoRegular::calcularPromedio(int estudianteId) const {
    return promedioPonderado(estudianteId);
}

std::string CursoTaller::tipo() const {
    return "taller";
}

std::optional<double> CursoTaller::calcularPromedio(int estudianteId) const {
    return promedioCategoria(buscarMatricula(estudianteId), TipoEvaluacion::Practica);
}

bool CursoTaller::aceptaTipo(TipoEvaluacion tipo) const noexcept {
    return tipo == TipoEvaluacion::Practica;
}

std::string CursoTeoricoPractico::tipo() const {
    return "teorico_practico";
}

std::optional<double> CursoTeoricoPractico::calcularPromedio(int estudianteId) const {
    return promedioPonderado(estudianteId);
}

int Instituto::registrarEstudiante(const std::string& nombre, const std::string& programa) {
    const int id = siguienteEstudiante_;
    Estudiante nuevo(id, nombre, programa);
    estudiantes_.emplace(id, std::move(nuevo));
    ++siguienteEstudiante_;
    return id;
}

int Instituto::registrarProfesor(const std::string& nombre,
                                 const std::string& especialidad) {
    const int id = siguienteProfesor_;
    Profesor nuevo(id, nombre, especialidad);
    profesores_.emplace(id, std::move(nuevo));
    ++siguienteProfesor_;
    return id;
}

int Instituto::registrarAsignatura(const std::string& nombre) {
    const int id = siguienteAsignatura_;
    Asignatura nueva(id, nombre);
    asignaturas_.emplace(id, std::move(nueva));
    ++siguienteAsignatura_;
    return id;
}

int Instituto::crearCurso(const std::string& tipo, int asignaturaId, int profesorId,
                          const std::string& semestre, const std::string& horario,
                          const std::string& aula) {
    asignatura(asignaturaId);
    profesor(profesorId);

    const int id = siguienteCurso_;
    std::unique_ptr<Curso> nuevo;
    if (tipo == "regular") {
        nuevo = std::make_unique<CursoRegular>(id, asignaturaId, profesorId, semestre,
                                               horario, aula);
    } else if (tipo == "taller") {
        nuevo = std::make_unique<CursoTaller>(id, asignaturaId, profesorId, semestre,
                                              horario, aula);
    } else if (tipo == "teorico_practico") {
        nuevo = std::make_unique<CursoTeoricoPractico>(
            id, asignaturaId, profesorId, semestre, horario, aula);
    } else {
        throw Error(400, "El tipo de curso no es válido.");
    }

    cursos_.emplace(id, std::move(nuevo));
    ++siguienteCurso_;
    return id;
}

void Instituto::matricular(int cursoId, int estudianteId) {
    estudiante(estudianteId);
    cursoMutable(cursoId).matricular(estudianteId);
}

void Instituto::registrarNota(int cursoId, int profesorId, int estudianteId,
                              TipoEvaluacion tipo, double nota) {
    const auto& docente = profesor(profesorId);
    docente.registrarNota(cursoMutable(cursoId), estudianteId, tipo, nota);
}

const Estudiante& Instituto::estudiante(int id) const {
    return buscar(estudiantes_, id, "Estudiante");
}

const Profesor& Instituto::profesor(int id) const {
    return buscar(profesores_, id, "Profesor");
}

const Asignatura& Instituto::asignatura(int id) const {
    return buscar(asignaturas_, id, "Asignatura");
}

const Curso& Instituto::curso(int id) const {
    const auto encontrado = cursos_.find(id);
    if (encontrado == cursos_.end()) {
        throw Error(404, "Curso no encontrado.");
    }
    return *encontrado->second;
}

Curso& Instituto::cursoMutable(int id) {
    const auto encontrado = cursos_.find(id);
    if (encontrado == cursos_.end()) {
        throw Error(404, "Curso no encontrado.");
    }
    return *encontrado->second;
}

const std::map<int, Estudiante>& Instituto::estudiantes() const noexcept {
    return estudiantes_;
}

const std::map<int, Profesor>& Instituto::profesores() const noexcept {
    return profesores_;
}

const std::map<int, Asignatura>& Instituto::asignaturas() const noexcept {
    return asignaturas_;
}

std::vector<const Curso*> Instituto::cursos() const {
    std::vector<const Curso*> resultado;
    resultado.reserve(cursos_.size());
    for (const auto& entrada : cursos_) {
        resultado.push_back(entrada.second.get());
    }
    return resultado;
}

Reporte ServicioReportes::generar(const Instituto& instituto, const std::string& semestre,
                                  const std::string& programa) {
    struct AcumuladoEstudiante {
        FilaEstudianteReporte fila;
        double suma = 0.0;
    };

    Reporte reporte;
    std::map<int, AcumuladoEstudiante> estudiantes;
    double sumaGeneral = 0.0;

    for (const Curso* curso : instituto.cursos()) {
        if (!semestre.empty() && curso->semestre() != semestre) {
            continue;
        }
        FilaCursoReporte fila;
        fila.cursoId = curso->id();
        double sumaCurso = 0.0;

        for (const auto& matricula : curso->matriculas()) {
            const auto& estudiante = instituto.estudiante(matricula.estudianteId());
            if (!programa.empty() && estudiante.programa() != programa) {
                continue;
            }
            auto [posicion, insertado] = estudiantes.try_emplace(estudiante.id());
            if (insertado) {
                posicion->second.fila.estudianteId = estudiante.id();
            }
            auto& acumulado = posicion->second;
            ++acumulado.fila.cursos;
            ++fila.matriculas;
            ++reporte.matriculas;

            const auto promedio = estudiante.consultarPromedio(*curso);
            if (!promedio) {
                ++reporte.pendientes;
                continue;
            }
            ++fila.completos;
            ++acumulado.fila.completos;
            ++reporte.completos;
            sumaCurso += *promedio;
            acumulado.suma += *promedio;
            sumaGeneral += *promedio;
            if (estaEnRiesgo(promedio)) {
                ++fila.alertas;
                ++acumulado.fila.alertas;
                ++reporte.alertas;
            }
        }

        if (fila.completos > 0) {
            fila.promedio = sumaCurso / static_cast<double>(fila.completos);
        }
        if (programa.empty() || fila.matriculas > 0) {
            reporte.porCurso.push_back(fila);
        }
    }

    for (auto& entrada : estudiantes) {
        auto& acumulado = entrada.second;
        if (acumulado.fila.completos > 0) {
            acumulado.fila.promedio =
                acumulado.suma / static_cast<double>(acumulado.fila.completos);
        }
        reporte.porEstudiante.push_back(acumulado.fila);
    }
    reporte.estudiantes = reporte.porEstudiante.size();
    reporte.cursos = reporte.porCurso.size();
    if (reporte.completos > 0) {
        reporte.promedioGeneral = sumaGeneral / static_cast<double>(reporte.completos);
    }
    return reporte;
}

Instituto crearDatosDemostracion() {
    Instituto instituto;
    const int ana = instituto.registrarEstudiante("Ana Pérez", "Ingeniería de Sistemas");
    const int luis = instituto.registrarEstudiante("Luis Mendoza", "Ingeniería de Sistemas");
    const int camila = instituto.registrarEstudiante("Camila Rojas", "Administración");
    const int maria = instituto.registrarProfesor("María Torres", "Programación");
    const int carlos = instituto.registrarProfesor("Carlos Fernández", "Laboratorio");
    const int programacion = instituto.registrarAsignatura("Programación orientada a objetos");
    const int laboratorio = instituto.registrarAsignatura("Taller de laboratorio");
    const int matematica = instituto.registrarAsignatura("Matemática aplicada");
    const int regular = instituto.crearCurso("regular", programacion, maria, "2026-2",
                                              "Lunes 08:00", "Aula 101");
    const int taller = instituto.crearCurso("taller", laboratorio, carlos, "2026-2",
                                             "Miércoles 10:00", "Laboratorio 2");
    const int mixto = instituto.crearCurso("teorico_practico", matematica, maria, "2026-2",
                                            "Viernes 08:00", "Aula 203");

    instituto.matricular(regular, ana);
    instituto.matricular(regular, luis);
    instituto.matricular(taller, ana);
    instituto.matricular(taller, camila);
    instituto.matricular(mixto, luis);
    instituto.registrarNota(regular, maria, ana, TipoEvaluacion::Parcial, 12);
    instituto.registrarNota(regular, maria, ana, TipoEvaluacion::Practica, 14);
    instituto.registrarNota(regular, maria, ana, TipoEvaluacion::ExamenFinal, 16);
    instituto.registrarNota(regular, maria, luis, TipoEvaluacion::Parcial, 9);
    instituto.registrarNota(regular, maria, luis, TipoEvaluacion::Practica, 10);
    instituto.registrarNota(regular, maria, luis, TipoEvaluacion::ExamenFinal, 10);
    instituto.registrarNota(taller, carlos, ana, TipoEvaluacion::Practica, 14);
    instituto.registrarNota(taller, carlos, camila, TipoEvaluacion::Practica, 18);
    instituto.registrarNota(mixto, maria, luis, TipoEvaluacion::Parcial, 13);
    return instituto;
}

}  // namespace innova
