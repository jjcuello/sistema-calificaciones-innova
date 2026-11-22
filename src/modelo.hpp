#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace innova {

class Error : public std::runtime_error {
public:
    Error(int estado, const std::string& mensaje);
    int estado() const noexcept;

private:
    int estado_;
};

enum class TipoEvaluacion {
    Parcial,
    Practica,
    ExamenFinal
};

TipoEvaluacion tipoEvaluacionDesdeTexto(const std::string& texto);
std::string textoTipoEvaluacion(TipoEvaluacion tipo);
bool estaEnRiesgo(const std::optional<double>& promedio) noexcept;

struct Evaluacion {
    TipoEvaluacion tipo;
    double nota;
};

class Curso;

class Estudiante {
public:
    Estudiante(int id, std::string nombre, std::string programa);

    int id() const noexcept;
    const std::string& nombre() const noexcept;
    const std::string& programa() const noexcept;
    std::vector<Evaluacion> consultarNotas(const Curso& curso) const;
    std::optional<double> consultarPromedio(const Curso& curso) const;

private:
    int id_;
    std::string nombre_;
    std::string programa_;
};

class Profesor {
public:
    Profesor(int id, std::string nombre, std::string especialidad);

    int id() const noexcept;
    const std::string& nombre() const noexcept;
    const std::string& especialidad() const noexcept;
    bool estaAsignado(const Curso& curso) const noexcept;
    void registrarNota(Curso& curso, int estudianteId, TipoEvaluacion tipo, double nota) const;

private:
    int id_;
    std::string nombre_;
    std::string especialidad_;
};

class Asignatura {
public:
    Asignatura(int id, std::string nombre);

    int id() const noexcept;
    const std::string& nombre() const noexcept;

private:
    int id_;
    std::string nombre_;
};

class Matricula {
public:
    int estudianteId() const noexcept;
    const std::vector<Evaluacion>& evaluaciones() const noexcept;

private:
    explicit Matricula(int estudianteId);
    void agregarEvaluacion(TipoEvaluacion tipo, double nota);

    int estudianteId_;
    std::vector<Evaluacion> evaluaciones_;

    friend class Curso;
};

class Curso {
public:
    static constexpr std::size_t Capacidad = 40;
    static constexpr std::size_t MaxEvaluacionesPorCategoria = 100;

    Curso(int id, int asignaturaId, int profesorId, std::string semestre,
          std::string horario, std::string aula);
    virtual ~Curso() = default;

    int id() const noexcept;
    int asignaturaId() const noexcept;
    int profesorId() const noexcept;
    const std::string& semestre() const noexcept;
    const std::string& horario() const noexcept;
    const std::string& aula() const noexcept;
    const std::vector<Matricula>& matriculas() const noexcept;

    void matricular(int estudianteId);
    void registrarNota(int profesorId, int estudianteId, TipoEvaluacion tipo, double nota);
    const std::vector<Evaluacion>& consultarNotas(int estudianteId) const;

    virtual std::string tipo() const = 0;
    virtual std::optional<double> calcularPromedio(int estudianteId) const = 0;

protected:
    const Matricula& buscarMatricula(int estudianteId) const;
    std::optional<double> promedioPonderado(int estudianteId) const;
    static std::optional<double> promedioCategoria(const Matricula& matricula,
                                                    TipoEvaluacion tipo);
    virtual bool aceptaTipo(TipoEvaluacion tipo) const noexcept;

private:
    int id_;
    int asignaturaId_;
    int profesorId_;
    std::string semestre_;
    std::string horario_;
    std::string aula_;
    std::vector<Matricula> matriculas_;
};

class CursoRegular final : public Curso {
public:
    using Curso::Curso;
    std::string tipo() const override;
    std::optional<double> calcularPromedio(int estudianteId) const override;
};

class CursoTaller final : public Curso {
public:
    using Curso::Curso;
    std::string tipo() const override;
    std::optional<double> calcularPromedio(int estudianteId) const override;

protected:
    bool aceptaTipo(TipoEvaluacion tipo) const noexcept override;
};

class CursoTeoricoPractico final : public Curso {
public:
    using Curso::Curso;
    std::string tipo() const override;
    std::optional<double> calcularPromedio(int estudianteId) const override;
};

class Instituto {
public:
    int registrarEstudiante(const std::string& nombre, const std::string& programa);
    int registrarProfesor(const std::string& nombre, const std::string& especialidad);
    int registrarAsignatura(const std::string& nombre);
    int crearCurso(const std::string& tipo, int asignaturaId, int profesorId,
                   const std::string& semestre, const std::string& horario,
                   const std::string& aula);
    void matricular(int cursoId, int estudianteId);
    void registrarNota(int cursoId, int profesorId, int estudianteId,
                       TipoEvaluacion tipo, double nota);

    const Estudiante& estudiante(int id) const;
    const Profesor& profesor(int id) const;
    const Asignatura& asignatura(int id) const;
    const Curso& curso(int id) const;
    const std::map<int, Estudiante>& estudiantes() const noexcept;
    const std::map<int, Profesor>& profesores() const noexcept;
    const std::map<int, Asignatura>& asignaturas() const noexcept;
    std::vector<const Curso*> cursos() const;

private:
    Curso& cursoMutable(int id);

    int siguienteEstudiante_ = 1;
    int siguienteProfesor_ = 1;
    int siguienteAsignatura_ = 1;
    int siguienteCurso_ = 1;
    std::map<int, Estudiante> estudiantes_;
    std::map<int, Profesor> profesores_;
    std::map<int, Asignatura> asignaturas_;
    std::map<int, std::unique_ptr<Curso>> cursos_;
};

struct FilaCursoReporte {
    int cursoId = 0;
    std::size_t matriculas = 0;
    std::size_t completos = 0;
    std::size_t alertas = 0;
    std::optional<double> promedio;
};

struct FilaEstudianteReporte {
    int estudianteId = 0;
    std::size_t cursos = 0;
    std::size_t completos = 0;
    std::size_t alertas = 0;
    std::optional<double> promedio;
};

struct Reporte {
    std::size_t matriculas = 0;
    std::size_t estudiantes = 0;
    std::size_t cursos = 0;
    std::size_t completos = 0;
    std::size_t pendientes = 0;
    std::size_t alertas = 0;
    std::optional<double> promedioGeneral;
    std::vector<FilaCursoReporte> porCurso;
    std::vector<FilaEstudianteReporte> porEstudiante;
};

class ServicioReportes {
public:
    static Reporte generar(const Instituto& instituto, const std::string& semestre = "",
                           const std::string& programa = "");
};

Instituto crearDatosDemostracion();

}  // namespace innova
