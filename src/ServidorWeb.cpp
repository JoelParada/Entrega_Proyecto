#include "ServidorWeb.h"
#include "Pacientes.h"
#include "MicroSD.h"
#include "Sesiones.h"
#include "Doctores.h"
#include <WiFi.h>
#include <WebServer.h>
#include "ModoAhorro.h"

const char* ssid = "MonitorCardiaco";
const char* password = "12345678";

WebServer server(80);

static int bpmActual = 0;
static int spo2Actual = -1;
static float tempActual = 0.0f;

// ================================================================
// BUFFER ECG
// ================================================================

const int TAM_BUFFER_ECG = 100;

static int bufferECG[TAM_BUFFER_ECG];
static volatile int indiceEscrituraECG = 0;
static volatile int cantidadMuestrasECG = 0;

static bool pacienteRegistrado = false;
static String cedulaPacienteActual = "";

static const char* COOKIE_SESION = "doctor_session=activa";

// ================================================================
// HTML
// ================================================================

const char pagina[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width,initial-scale=1"/>
  <title>Monitor de Salud</title>

  <style>
    :root{
      --p:#111c2e;
      --p2:#17243a;
      --t:#f8fafc;
      --m:#93a4bb;
      --g:#39ff88;
      --c:#36d7ff;
      --r:#ff4d67;
      --o:#ffb347;
    }

    *{box-sizing:border-box}

    body{
      margin:0;
      min-height:100vh;
      background:linear-gradient(#06101d,#0b1525);
      color:var(--t);
      font-family:Arial;
    }

    .container{
      max-width:980px;
      margin:auto;
      padding:14px;
    }

    .card{
      background:linear-gradient(var(--p2),var(--p));
      border:1px solid #263753;
      border-radius:18px;
      padding:18px;
      margin-bottom:12px;
    }

    .login{
      max-width:460px;
      margin:70px auto;
    }

    .oculto{display:none!important}

    header,.doctorbar{
      display:flex;
      justify-content:space-between;
      align-items:center;
      gap:12px;
      margin-bottom:14px;
    }

    .badge{
      padding:8px 14px;
      border-radius:999px;
      background:#14351f;
      color:#8affb0;
      font-weight:bold;
    }

    .grid{
      display:grid;
      grid-template-columns:repeat(3,1fr);
      gap:12px;
    }

    .metric{text-align:center}
    .label{color:var(--m);font-size:14px}
    .value{font-size:50px;font-weight:800}
    .bpm{color:var(--r)}
    .spo2{color:var(--c)}
    .temp{color:var(--o)}

    .form{
      display:grid;
      grid-template-columns:1fr 1fr;
      gap:12px;
    }

    .full{grid-column:1/3}

    label{
      display:block;
      color:var(--m);
      font-size:13px;
      margin:8px 0 6px;
    }

    input,select,textarea{
      width:100%;
      padding:11px;
      border-radius:10px;
      border:1px solid #334764;
      background:#091426;
      color:white;
    }

    button{
      width:100%;
      margin-top:12px;
      padding:13px;
      border:0;
      border-radius:11px;
      color:white;
      font-weight:bold;
      font-size:16px;
      cursor:pointer;
    }

    button:disabled{background:#475569!important;cursor:not-allowed}

    #entrar,#guardar{background:#16a34a}
    #registrarDoctor{background:#0284c7}
    #iniciar{background:#0284c7}
    #finalizar,#cerrar{background:#dc2626}

    .doctorbar button{
      width:auto;
      margin:0;
      padding:9px 13px;
    }

    .status{
      display:grid;
      grid-template-columns:1fr 1fr;
      gap:10px;
    }

    .item{
      background:#091426;
      border:1px solid #334764;
      border-radius:10px;
      padding:11px;
    }

    .muted{color:var(--m)}

    .acciones{
      display:grid;
      grid-template-columns:1fr 1fr;
      gap:12px;
    }

    .mensaje{
      text-align:center;
      min-height:20px;
      margin-top:10px;
    }

    .ecg{grid-column:1/4}

    canvas{
      width:100%;
      height:260px;
      background:#05100a;
      border-radius:12px;
    }

    .enlace{
      display:block;
      margin-top:14px;
      text-align:center;
      color:var(--c);
      cursor:pointer;
      text-decoration:underline;
    }

    .tabs{
      display:flex;
      gap:8px;
      margin-bottom:14px;
    }

    .tab{
      flex:1;
      background:#0b1525;
      border:1px solid #334764;
      color:var(--m);
      padding:11px;
      border-radius:11px;
      cursor:pointer;
      font-weight:bold;
      text-align:center;
    }

    .tab.activo{
      background:#16a34a;
      color:white;
      border-color:#16a34a;
    }

    .tabla-contenedor{
      overflow-x:auto;
    }

    table{
      width:100%;
      border-collapse:collapse;
      margin-top:10px;
      min-width:760px;
    }

    th,td{
      padding:10px;
      border-bottom:1px solid #263753;
      text-align:left;
      vertical-align:top;
      font-size:13px;
    }

    th{
      color:var(--m);
      background:#091426;
    }

    .estado-pendiente{
      color:var(--o);
      font-weight:bold;
    }

    .estado-revisado{
      color:#8affb0;
      font-weight:bold;
    }

    .btn-revisar{
      width:auto;
      margin:0;
      padding:7px 10px;
      font-size:13px;
      background:#0284c7;
    }

    @media(max-width:700px){
      .grid{grid-template-columns:1fr 1fr}
      .grid article:nth-child(3),.ecg{grid-column:1/3}
      .form,.status,.acciones{grid-template-columns:1fr}
      .full{grid-column:1}
      .tabs{flex-direction:column}
    }
  </style>
</head>

<body>
  <div class="container">

    <!-- ======================================================= -->
    <!-- LOGIN / REGISTRO DE DOCTOR                              -->
    <!-- ======================================================= -->

    <section class="card login" id="loginVista">

      <div id="formLogin">
        <h1>Acceso médico</h1>

        <p class="muted">
          Inicie sesión para consultar sensores y administrar pacientes.
        </p>

        <label>Usuario</label>
        <input id="usuarioLogin"/>

        <label>Contraseña</label>
        <input id="passwordLogin" type="password"/>

        <button id="entrar" onclick="login()">
          Iniciar sesión
        </button>

        <div class="mensaje" id="mensajeLogin"></div>

        <span class="enlace" onclick="mostrarRegistro(true)">
          ¿No tiene una cuenta? Registrar nuevo doctor
        </span>
      </div>

      <div class="oculto" id="formRegistro">
        <h1>Registrar doctor</h1>

        <p class="muted">
          Cree una cuenta local almacenada en la MicroSD.
        </p>

        <label>Nombre completo</label>
        <input id="nombreDoctor"/>

        <label>Usuario</label>
        <input id="usuarioDoctor"/>

        <label>Contraseña</label>
        <input id="passwordDoctor" type="password"/>

        <label>Confirmar contraseña</label>
        <input id="confirmarPasswordDoctor" type="password"/>

        <button id="registrarDoctor" onclick="registrarDoctorNuevo()">
          Registrar doctor
        </button>

        <div class="mensaje" id="mensajeRegistro"></div>

        <span class="enlace" onclick="mostrarRegistro(false)">
          Volver al inicio de sesión
        </span>
      </div>

    </section>

    <!-- ======================================================= -->
    <!-- PANEL PRINCIPAL                                         -->
    <!-- ======================================================= -->

    <div class="oculto" id="panel">

      <div class="doctorbar">
        <div>
          <strong id="doctorNombre">Doctor</strong>
          <div class="muted" id="doctorId"></div>
        </div>

        <button id="cerrar" onclick="logout()">
          Cerrar sesión
        </button>
      </div>

      <div class="tabs">
        <div class="tab activo" id="tabMonitor" onclick="cambiarTab('monitor')">
          Monitor y paciente
        </div>

        <div class="tab" id="tabPacientes" onclick="cambiarTab('pacientes')">
          Mis pacientes
        </div>
      </div>

      <!-- ===================================================== -->
      <!-- TAB MONITOR                                           -->
      <!-- ===================================================== -->

      <div id="vistaMonitor">

        <header>
          <h1>Monitor de Salud ESP32</h1>
          <div class="badge" id="estado">NORMAL</div>
        </header>

        <section class="card">
          <h2>Registro de paciente</h2>

          <div class="muted" id="sd">
            Comprobando MicroSD...
          </div>

          <div class="form">
            <div>
              <label>Nombre completo</label>
              <input id="nombre"/>
            </div>

            <div>
              <label>Cédula</label>
              <input id="cedula"/>
            </div>

            <div>
              <label>Edad</label>
              <input id="edad" type="number" min="1" max="120"/>
            </div>

            <div>
              <label>Sexo</label>
              <select id="sexo">
                <option value="">Seleccione</option>
                <option>Masculino</option>
                <option>Femenino</option>
                <option>Otro</option>
              </select>
            </div>

            <div class="full">
              <label>Observaciones</label>
              <textarea id="observaciones" rows="3"></textarea>
            </div>
          </div>

          <button id="guardar" onclick="guardarPaciente()">
            Guardar paciente
          </button>

          <div class="mensaje" id="mensajePaciente"></div>
        </section>

        <section class="card">
          <h2>Control del examen</h2>

          <div class="status">
            <div class="item">
              <div class="muted">Paciente actual</div>
              <strong id="pacienteActual">No registrado</strong>
            </div>

            <div class="item">
              <div class="muted">Estado</div>
              <strong id="estadoSesion">ESPERANDO</strong>
            </div>
          </div>

          <div class="acciones">
            <button id="iniciar" onclick="iniciarExamen()" disabled>
              Iniciar examen
            </button>

            <button id="finalizar" onclick="finalizarExamen()" disabled>
              Finalizar examen
            </button>
          </div>

          <div class="mensaje" id="mensajeSesion"></div>
        </section>

        <section class="grid">
          <article class="card metric">
            <div class="label">Frecuencia cardíaca</div>
            <div class="value bpm" id="bpm">0</div>
            <div>BPM</div>
          </article>

          <article class="card metric">
            <div class="label">SpO₂</div>
            <div class="value spo2" id="spo2">--</div>
            <div>%</div>
          </article>

          <article class="card metric">
            <div class="label">Temperatura</div>
            <div class="value temp" id="temp">0.0</div>
            <div>°C</div>
          </article>

          <article class="card ecg">
            <h3>ECG en tiempo real</h3>
            <canvas id="ecg"></canvas>
          </article>
        </section>
      </div>

      <!-- ===================================================== -->
      <!-- TAB MIS PACIENTES                                     -->
      <!-- ===================================================== -->

      <div class="oculto" id="vistaPacientes">
        <section class="card">
          <h2>Pacientes atendidos</h2>

          <p class="muted">
            Solo se muestran los pacientes asociados al doctor autenticado.
          </p>

          <button onclick="cargarPacientes()" style="background:#0284c7">
            Actualizar lista
          </button>

          <div class="mensaje" id="mensajeLista"></div>

          <div class="tabla-contenedor" id="contenidoPacientes">
            <p class="muted">Todavía no se ha cargado la lista.</p>
          </div>
        </section>
      </div>

    </div>
  </div>

  <script>
    const cv = document.getElementById('ecg');
    const cx = cv.getContext('2d');
    const pts = [];

    let auth = false;

    const loginVista = document.getElementById('loginVista');
    const formLogin = document.getElementById('formLogin');
    const formRegistro = document.getElementById('formRegistro');
    const panel = document.getElementById('panel');

    const usuarioLogin = document.getElementById('usuarioLogin');
    const passwordLogin = document.getElementById('passwordLogin');
    const mensajeLogin = document.getElementById('mensajeLogin');

    const nombreDoctorInput = document.getElementById('nombreDoctor');
    const usuarioDoctor = document.getElementById('usuarioDoctor');
    const passwordDoctor = document.getElementById('passwordDoctor');
    const confirmarPasswordDoctor = document.getElementById('confirmarPasswordDoctor');
    const mensajeRegistro = document.getElementById('mensajeRegistro');

    const doctorNombre = document.getElementById('doctorNombre');
    const doctorId = document.getElementById('doctorId');

    const tabMonitor = document.getElementById('tabMonitor');
    const tabPacientes = document.getElementById('tabPacientes');
    const vistaMonitor = document.getElementById('vistaMonitor');
    const vistaPacientes = document.getElementById('vistaPacientes');

    const nombre = document.getElementById('nombre');
    const cedula = document.getElementById('cedula');
    const edad = document.getElementById('edad');
    const sexo = document.getElementById('sexo');
    const observaciones = document.getElementById('observaciones');

    const guardar = document.getElementById('guardar');
    const mensajePaciente = document.getElementById('mensajePaciente');

    const sd = document.getElementById('sd');
    const pacienteActual = document.getElementById('pacienteActual');
    const estadoSesion = document.getElementById('estadoSesion');
    const iniciar = document.getElementById('iniciar');
    const finalizar = document.getElementById('finalizar');
    const mensajeSesion = document.getElementById('mensajeSesion');

    const bpm = document.getElementById('bpm');
    const spo2 = document.getElementById('spo2');
    const temp = document.getElementById('temp');

    const contenidoPacientes = document.getElementById('contenidoPacientes');
    const mensajeLista = document.getElementById('mensajeLista');

    function resize()
    {
      cv.width = cv.clientWidth;
      cv.height = cv.clientHeight;
    }

    window.onresize = resize;

    function showLogin()
    {
      auth = false;
      loginVista.classList.remove('oculto');
      panel.classList.add('oculto');
    }

    function showPanel(d)
    {
      auth = true;
      loginVista.classList.add('oculto');
      panel.classList.remove('oculto');

      doctorNombre.textContent = d.doctorNombre;
      doctorId.textContent = d.doctorId;

      resize();
    }

    function mostrarRegistro(mostrar)
    {
      formLogin.classList.toggle('oculto', mostrar);
      formRegistro.classList.toggle('oculto', !mostrar);

      mensajeLogin.textContent = '';
      mensajeRegistro.textContent = '';
    }

    function cambiarTab(tab)
    {
      const monitorActivo = tab === 'monitor';

      vistaMonitor.classList.toggle('oculto', !monitorActivo);
      vistaPacientes.classList.toggle('oculto', monitorActivo);

      tabMonitor.classList.toggle('activo', monitorActivo);
      tabPacientes.classList.toggle('activo', !monitorActivo);

      if (!monitorActivo)
      {
        cargarPacientes();
      }
      else
      {
        resize();
        draw();
      }
    }

    function draw()
    {
      cx.clearRect(0, 0, cv.width, cv.height);

      if (pts.length < 2)
      {
        return;
      }

      let minimo = Math.min(...pts);
      let maximo = Math.max(...pts);

      if (maximo - minimo < 50)
      {
        minimo -= 25;
        maximo += 25;
      }

      const margen = (maximo - minimo) * 0.15;

      minimo -= margen;
      maximo += margen;

      cx.beginPath();
      cx.strokeStyle = '#39ff88';
      cx.lineWidth = 2;

      pts.forEach((v, i) =>
      {
        const x = i * cv.width / (pts.length - 1);
        const y =
          cv.height -
          ((v - minimo) / (maximo - minimo)) *
          cv.height;

        if (i === 0)
        {
          cx.moveTo(x, y);
        }
        else
        {
          cx.lineTo(x, y);
        }
      });

      cx.stroke();
    }

    async function registrarDoctorNuevo()
    {
      const nombreDoctor = nombreDoctorInput.value.trim();
      const usuario = usuarioDoctor.value.trim();
      const password = passwordDoctor.value;
      const confirmar = confirmarPasswordDoctor.value;

      mensajeRegistro.style.color = '#fca5a5';

      if (nombreDoctor.length < 3)
      {
        mensajeRegistro.textContent = 'Ingrese el nombre completo del doctor';
        return;
      }

      if (usuario.length < 3)
      {
        mensajeRegistro.textContent = 'El usuario debe tener al menos 3 caracteres';
        return;
      }

      if (password.length < 4)
      {
        mensajeRegistro.textContent = 'La contraseña debe tener al menos 4 caracteres';
        return;
      }

      if (password !== confirmar)
      {
        mensajeRegistro.textContent = 'Las contraseñas no coinciden';
        return;
      }

      mensajeRegistro.textContent = 'Registrando doctor...';

      const q = new URLSearchParams({
        nombre: nombreDoctor,
        usuario: usuario,
        password: password
      });

      try
      {
        const r = await fetch('/registrar-doctor',
        {
          method: 'POST',
          headers:
          {
            'Content-Type':'application/x-www-form-urlencoded'
          },
          body:q.toString()
        });

        const d = await r.json();

        mensajeRegistro.textContent = d.mensaje;
        mensajeRegistro.style.color = d.ok ? '#86efac' : '#fca5a5';

        if (d.ok)
        {
          usuarioLogin.value = usuario;

          nombreDoctorInput.value = '';
          usuarioDoctor.value = '';
          passwordDoctor.value = '';
          confirmarPasswordDoctor.value = '';

          setTimeout(() =>
          {
            mostrarRegistro(false);
            passwordLogin.focus();
          }, 1200);
        }
      }
      catch (e)
      {
        mensajeRegistro.textContent = 'No se pudo comunicar con la ESP32';
      }
    }

    async function login()
    {
      const q = new URLSearchParams({
        usuario:usuarioLogin.value.trim(),
        password:passwordLogin.value
      });

      mensajeLogin.textContent = 'Verificando...';

      try
      {
        const r = await fetch('/login',
        {
          method:'POST',
          headers:
          {
            'Content-Type':'application/x-www-form-urlencoded'
          },
          body:q.toString()
        });

        const d = await r.json();

        mensajeLogin.textContent = d.mensaje;
        mensajeLogin.style.color = d.ok ? '#86efac' : '#fca5a5';

        if (d.ok)
        {
          cambiarTab('monitor');
          update();
        }
      }
      catch (e)
      {
        mensajeLogin.textContent = 'No se pudo comunicar con la ESP32';
        mensajeLogin.style.color = '#fca5a5';
      }
    }

    async function logout()
    {
      const r = await fetch('/logout', {method:'POST'});
      const d = await r.json();

      if (!d.ok)
      {
        mensajeSesion.textContent = d.mensaje;
        return;
      }

      showLogin();
      mensajeLogin.textContent = 'Sesión cerrada';
      mensajeLogin.style.color = '#86efac';
    }

    async function update()
    {
      try
      {
        const r = await fetch('/datos', {cache:'no-store'});

        if (r.status === 401)
        {
          showLogin();
          return;
        }

        const d = await r.json();

        showPanel(d);

        bpm.textContent = d.bpm;
        spo2.textContent = d.spo2 > 0 ? d.spo2 : '--';
        temp.textContent = Number(d.temp).toFixed(1);

        sd.textContent =
          d.sd
            ? 'MicroSD conectada'
            : 'ADVERTENCIA: MicroSD no detectada';

        pacienteActual.textContent =
          d.paciente
            ? d.cedula
            : 'No registrado';

        estadoSesion.textContent =
          d.sesion
            ? 'GRABANDO'
            : 'ESPERANDO';

        guardar.disabled = !d.sd || d.sesion;
        iniciar.disabled = !d.sd || !d.paciente || d.sesion;
        finalizar.disabled = !d.sesion;

        if (Array.isArray(d.ecg))
        {
          d.ecg.forEach(v =>
          {
            pts.push(v);

            if (pts.length > 220)
            {
              pts.shift();
            }
          });
        }

        if (!vistaMonitor.classList.contains('oculto'))
        {
          draw();
        }
      }
      catch (e)
      {
      }
    }

    async function guardarPaciente()
    {
      const botonGuardar = document.getElementById("guardar");

      botonGuardar.disabled = true;
      botonGuardar.textContent = "Guardando...";

      mensajePaciente.textContent = "Guardando paciente...";
      mensajePaciente.style.color = "#93a4bb";

      const q = new URLSearchParams({
        nombre:nombre.value.trim(),
        cedula:cedula.value.trim(),
        edad:edad.value,
        sexo:sexo.value,
        observaciones:observaciones.value.trim()
      });

      try
      {
        const r = await fetch('/guardar-paciente',
        {
          method:'POST',
          headers:
          {
            'Content-Type':'application/x-www-form-urlencoded'
          },
          body:q.toString()
        });

        if (r.status === 401)
        {
          showLogin();
          return;
        }

        const d = await r.json();

        mensajePaciente.textContent = d.mensaje;
        mensajePaciente.style.color = d.ok ? '#86efac' : '#fca5a5';

        if (d.ok)
        {
          cargarPacientes();

          setTimeout(() =>
          {
            update();
          }, 500);
        }
      }
      catch (e)
      {
        mensajePaciente.textContent = "No se pudo comunicar con la ESP32";
        mensajePaciente.style.color = "#fca5a5";
      }
      finally
      {
        botonGuardar.disabled = false;
        botonGuardar.textContent = "Guardar paciente";
      }
    }

    async function iniciarExamen()
    {
      const botonIniciar = document.getElementById("iniciar");

      botonIniciar.disabled = true;
      botonIniciar.textContent = "Iniciando...";

      mensajeSesion.textContent = "Iniciando examen...";
      mensajeSesion.style.color = "#93a4bb";

      try
      {
        const r = await fetch('/iniciar-examen',
        {
          method:'POST'
        });

        if (r.status === 401)
        {
          showLogin();
          return;
        }

        const d = await r.json();

        mensajeSesion.textContent = d.mensaje;
        mensajeSesion.style.color = d.ok ? '#86efac' : '#fca5a5';

        if (d.ok)
        {
          setTimeout(() =>
          {
            update();
          }, 500);
        }
      }
      catch (e)
      {
        mensajeSesion.textContent = "No se pudo comunicar con la ESP32";
        mensajeSesion.style.color = "#fca5a5";
      }
      finally
      {
        botonIniciar.disabled = false;
        botonIniciar.textContent = "Iniciar examen";
      }
    }

    async function finalizarExamen()
    {
      const botonFinalizar = document.getElementById("finalizar");

      botonFinalizar.disabled = true;
      botonFinalizar.textContent = "Finalizando...";

      mensajeSesion.textContent = "Finalizando examen...";
      mensajeSesion.style.color = "#93a4bb";

      try
      {
        const r = await fetch('/finalizar-examen',
        {
          method:'POST'
        });

        if (r.status === 401)
        {
          showLogin();
          return;
        }

        const d = await r.json();

        mensajeSesion.textContent = d.mensaje;
        mensajeSesion.style.color = d.ok ? '#86efac' : '#fca5a5';

        if (d.ok)
        {
          cargarPacientes();

          setTimeout(() =>
          {
            update();
          }, 500);
        }
      }
      catch (e)
      {
        mensajeSesion.textContent = "No se pudo comunicar con la ESP32";
        mensajeSesion.style.color = "#fca5a5";
      }
      finally
      {
        botonFinalizar.disabled = false;
        botonFinalizar.textContent = "Finalizar examen";
      }
    }

    function escaparHTML(valor)
    {
      return String(valor == null ? '' : valor)
        .replace(/&/g,'&amp;')
        .replace(/</g,'&lt;')
        .replace(/>/g,'&gt;')
        .replace(/"/g,'&quot;')
        .replace(/'/g,'&#39;');
    }

    async function cargarPacientes()
    {
      mensajeLista.textContent = 'Cargando pacientes...';
      mensajeLista.style.color = '#93a4bb';

      try
      {
        const r = await fetch('/mis-pacientes', {cache:'no-store'});

        if (r.status === 401)
        {
          showLogin();
          return;
        }

        const d = await r.json();
        const lista = Array.isArray(d.pacientes) ? d.pacientes : [];

        if (!d.ok)
        {
          mensajeLista.textContent = d.mensaje || 'No se pudo cargar la lista';
          mensajeLista.style.color = '#fca5a5';
          return;
        }

        mensajeLista.textContent =
          lista.length + ' paciente(s) asociado(s) al doctor';

        mensajeLista.style.color = '#86efac';

        if (lista.length === 0)
        {
          contenidoPacientes.innerHTML =
            '<p class="muted">No hay pacientes registrados para este doctor.</p>';

          return;
        }

        let html =
          '<table>' +
          '<tr>' +
          '<th>Nombre</th>' +
          '<th>Cédula</th>' +
          '<th>Edad</th>' +
          '<th>Sexo</th>' +
          '<th>Observaciones</th>' +
          '<th>Estado</th>' +
          '<th>Sincronización</th>' +
          '<th>Acción</th>' +
          '</tr>';

        lista.forEach(p =>
        {
          const revisado = p.estado === 'revisado';

          html +=
            '<tr>' +
            '<td>' + escaparHTML(p.nombre) + '</td>' +
            '<td>' + escaparHTML(p.cedula) + '</td>' +
            '<td>' + escaparHTML(p.edad) + '</td>' +
            '<td>' + escaparHTML(p.sexo) + '</td>' +
            '<td>' + escaparHTML(p.observaciones) + '</td>' +
            '<td class="' +
              (revisado ? 'estado-revisado' : 'estado-pendiente') +
            '">' +
              escaparHTML(p.estado) +
            '</td>' +
            '<td>' + escaparHTML(p.sincronizacion) + '</td>' +
            '<td>' +
              (
                revisado
                  ? '<span class="muted">Sin acción</span>'
                  : '<button class="btn-revisar" onclick="marcarRevisado(\'' +
                    escaparHTML(p.cedula) +
                    '\')">Marcar revisado</button>'
              ) +
            '</td>' +
            '</tr>';
        });

        html += '</table>';

        contenidoPacientes.innerHTML = html;
      }
      catch (e)
      {
        mensajeLista.textContent = 'Error al comunicarse con la ESP32';
        mensajeLista.style.color = '#fca5a5';
      }
    }

    async function marcarRevisado(cedulaPaciente)
    {
      const q = new URLSearchParams({
        cedula:cedulaPaciente
      });

      const r = await fetch('/marcar-revisado',
      {
        method:'POST',
        headers:
        {
          'Content-Type':'application/x-www-form-urlencoded'
        },
        body:q.toString()
      });

      if (r.status === 401)
      {
        showLogin();
        return;
      }

      const d = await r.json();

      mensajeLista.textContent = d.mensaje;
      mensajeLista.style.color = d.ok ? '#86efac' : '#fca5a5';

      if (d.ok)
      {
        cargarPacientes();
      }
    }

    setInterval(() =>
    {
      if (auth)
      {
        update();
      }
    }, 150);

    update();
  </script>
</body>
</html>
)rawliteral";

// ================================================================
// UTILIDADES
// ================================================================

static String esc(String s)
{
    s.replace("\\", "\\\\");
    s.replace("\"", "\\\"");
    s.replace("\n", " ");
    s.replace("\r", " ");

    return s;
}

static void respuesta(
    int codigo,
    bool ok,
    const String& mensaje
)
{
    String json = "{\"ok\":";

    json += ok ? "true" : "false";
    json += ",\"mensaje\":\"";
    json += esc(mensaje);
    json += "\"}";

    server.sendHeader("Cache-Control", "no-store");
    server.send(codigo, "application/json", json);
}

static bool auth()
{
    return
        Doctor_autenticado() &&
        server.hasHeader("Cookie") &&
        server.header("Cookie").indexOf(COOKIE_SESION) >= 0;
}

static bool exigir()
{
    if (auth())
    {
        return true;
    }

    respuesta(
        401,
        false,
        "Debe iniciar sesión"
    );

    return false;
}

// ================================================================
// PAGINA PRINCIPAL
// ================================================================

static void paginaPrincipal()
{
    server.send_P(
        200,
        "text/html",
        pagina
    );
}

// ================================================================
// DATOS EN TIEMPO REAL
// ================================================================

static void datos()
{
    if (!exigir())
    {
        return;
    }

    String json = "{";

    json += "\"bpm\":" +
        String(bpmActual) +
        ",";

    json += "\"spo2\":" +
        String(spo2Actual) +
        ",";

    json += "\"temp\":" +
        String(tempActual, 1) +
        ",";

    // ============================================================
    // ECG
    // ============================================================

    json += "\"ecg\":[";

    noInterrupts();

    int cantidad =
        cantidadMuestrasECG;

    int inicio =
        indiceEscrituraECG - cantidad;

    if (inicio < 0)
    {
        inicio += TAM_BUFFER_ECG;
    }

    for (int i = 0; i < cantidad; i++)
    {
        int posicion =
            (inicio + i) %
            TAM_BUFFER_ECG;

        if (i > 0)
        {
            json += ",";
        }

        json += String(
            bufferECG[posicion]
        );
    }

    cantidadMuestrasECG = 0;

    interrupts();

    json += "],";

    // ============================================================
    // ESTADOS
    // ============================================================

    json += "\"sd\":";
    json +=
        MicroSD_disponible()
            ? "true"
            : "false";

    json += ",";

    json += "\"paciente\":";
    json +=
        pacienteRegistrado
            ? "true"
            : "false";

    json += ",";

    json += "\"cedula\":\"";
    json += esc(
        cedulaPacienteActual
    );
    json += "\",";

    json += "\"sesion\":";
    json +=
        Sesion_activa()
            ? "true"
            : "false";

    json += ",";

    json += "\"doctorId\":\"";
    json += esc(
        Doctor_getId()
    );
    json += "\",";

    json += "\"doctorNombre\":\"";
    json += esc(
        Doctor_getNombre()
    );
    json += "\"}";

    server.sendHeader(
        "Cache-Control",
        "no-store"
    );

    server.send(
        200,
        "application/json",
        json
    );
}

// ================================================================
// REGISTRAR DOCTOR
// ================================================================

static void registrarDoctorWeb()
{
    if (!MicroSD_disponible())
    {
        respuesta(
            503,
            false,
            "MicroSD no disponible"
        );

        return;
    }

    if (!server.hasArg("nombre") ||
        !server.hasArg("usuario") ||
        !server.hasArg("password"))
    {
        respuesta(
            400,
            false,
            "Datos incompletos"
        );

        return;
    }

    String nombre =
        server.arg("nombre");

    String usuario =
        server.arg("usuario");

    String passwordDoctor =
        server.arg("password");

    nombre.trim();
    usuario.trim();

    if (Doctor_usuarioExiste(usuario))
    {
        respuesta(
            409,
            false,
            "Ese nombre de usuario ya existe"
        );

        return;
    }

    if (!Doctor_registrar(
            nombre,
            usuario,
            passwordDoctor))
    {
        respuesta(
            500,
            false,
            "No se pudo registrar el doctor"
        );

        return;
    }

    respuesta(
        201,
        true,
        "Doctor registrado correctamente"
    );
}

// ================================================================
// LOGIN
// ================================================================

static void loginDoctor()
{
    if (!server.hasArg("usuario") ||
        !server.hasArg("password"))
    {
        respuesta(
            400,
            false,
            "Datos incompletos"
        );

        return;
    }

    if (!Doctor_iniciarSesion(
            server.arg("usuario"),
            server.arg("password")))
    {
        respuesta(
            401,
            false,
            "Usuario o contraseña incorrectos"
        );

        return;
    }

    pacienteRegistrado = false;
    cedulaPacienteActual = "";

    server.sendHeader(
        "Set-Cookie",
        String(COOKIE_SESION) +
        "; Path=/; SameSite=Strict"
    );

    respuesta(
        200,
        true,
        "Inicio de sesión correcto"
    );
}

// ================================================================
// LOGOUT
// ================================================================

static void logoutDoctor()
{
    if (Sesion_activa())
    {
        respuesta(
            409,
            false,
            "Finalice el examen antes de cerrar sesión"
        );

        return;
    }

    Doctor_cerrarSesion();

    pacienteRegistrado = false;
    cedulaPacienteActual = "";

    server.sendHeader(
        "Set-Cookie",
        "doctor_session=; Path=/; Max-Age=0; SameSite=Strict"
    );

    respuesta(
        200,
        true,
        "Sesión cerrada"
    );
}

// ================================================================
// GUARDAR PACIENTE
// ================================================================

static void guardarPacienteWeb()
{
    if (!exigir())
    {
        return;
    }

    if (Sesion_activa())
    {
        respuesta(
            409,
            false,
            "Finalice la sesión antes de cambiar de paciente"
        );

        return;
    }

    if (!MicroSD_disponible())
    {
        respuesta(
            503,
            false,
            "MicroSD no detectada"
        );

        return;
    }

    if (!server.hasArg("nombre") ||
        !server.hasArg("cedula") ||
        !server.hasArg("edad") ||
        !server.hasArg("sexo"))
    {
        respuesta(
            400,
            false,
            "Faltan campos obligatorios"
        );

        return;
    }

    String nombrePaciente =
        server.arg("nombre");

    String cedulaPaciente =
        server.arg("cedula");

    String sexoPaciente =
        server.arg("sexo");

    String observacionesPaciente =
        server.hasArg("observaciones")
            ? server.arg("observaciones")
            : "";

    bool ok =
        Paciente_guardar(
            nombrePaciente,
            cedulaPaciente,
            server.arg("edad").toInt(),
            sexoPaciente,
            observacionesPaciente,
            Doctor_getId(),
            Doctor_getNombre()
        );

    if (!ok)
    {
        respuesta(
            500,
            false,
            "No se pudo guardar el paciente"
        );

        return;
    }

    cedulaPacienteActual =
        cedulaPaciente;

    pacienteRegistrado = true;

    ModoAhorro_registrarActividad();

    respuesta(
        200,
        true,
        "Paciente guardado y asociado al doctor"
    );
}

// ================================================================
// LISTAR PACIENTES DEL DOCTOR
// ================================================================

static void listarPacientesWeb()
{
    if (!exigir())
    {
        return;
    }

    String json =
        Paciente_listarPorDoctorJSON(
            Doctor_getId()
        );

    server.sendHeader(
        "Cache-Control",
        "no-store"
    );

    server.send(
        200,
        "application/json",
        json
    );
}

// ================================================================
// MARCAR PACIENTE COMO REVISADO
// ================================================================

static void marcarRevisadoWeb()
{
    if (!exigir())
    {
        return;
    }

    if (!server.hasArg("cedula"))
    {
        respuesta(
            400,
            false,
            "Falta la cédula"
        );

        return;
    }

    if (!Paciente_marcarRevisado(
            server.arg("cedula"),
            Doctor_getId()))
    {
        respuesta(
            500,
            false,
            "No se pudo actualizar el paciente"
        );

        return;
    }

    respuesta(
        200,
        true,
        "Paciente marcado como revisado"
    );
}

// ================================================================
// SESIONES
// ================================================================

static void iniciarExamenWeb()
{
    if (!exigir())
    {
        return;
    }

    if (!MicroSD_disponible())
    {
        respuesta(
            503,
            false,
            "MicroSD no detectada"
        );

        return;
    }

    if (!pacienteRegistrado)
    {
        respuesta(
            400,
            false,
            "Primero registre al paciente"
        );

        return;
    }

    if (Sesion_activa())
    {
        respuesta(
            409,
            false,
            "Ya existe una sesión activa"
        );

        return;
    }

    if (!Sesion_iniciar(
            cedulaPacienteActual))
    {
        respuesta(
            500,
            false,
            "No se pudo iniciar la sesión"
        );

        return;
    }

    ModoAhorro_registrarActividad();

    respuesta(
        200,
        true,
        "Examen iniciado"
    );
}

static void finalizarExamenWeb()
{
    if (!exigir())
    {
        return;
    }

    if (!Sesion_activa())
    {
        respuesta(
            409,
            false,
            "No existe una sesión activa"
        );

        return;
    }

    if (!Sesion_finalizar())
    {
        respuesta(
            500,
            false,
            "No se pudo finalizar la sesión"
        );

        return;
    }

    ModoAhorro_registrarActividad();

    respuesta(
        200,
        true,
        "Examen finalizado y guardado"
    );
}

// ================================================================
// INICIALIZAR SERVIDOR
// ================================================================

void WebServer_begin()
{
    WiFi.mode(WIFI_AP_STA);

    if (!WiFi.softAP(
            ssid,
            password))
    {
        Serial.println(
            "ERROR WiFi"
        );

        return;
    }

    const char* headers[] =
    {
        "Cookie"
    };

    server.collectHeaders(
        headers,
        1
    );

    server.on(
        "/",
        HTTP_GET,
        paginaPrincipal
    );

    server.on(
        "/datos",
        HTTP_GET,
        datos
    );

    server.on(
        "/registrar-doctor",
        HTTP_POST,
        registrarDoctorWeb
    );

    server.on(
        "/login",
        HTTP_POST,
        loginDoctor
    );

    server.on(
        "/logout",
        HTTP_POST,
        logoutDoctor
    );

    server.on(
        "/guardar-paciente",
        HTTP_POST,
        guardarPacienteWeb
    );

    server.on(
        "/mis-pacientes",
        HTTP_GET,
        listarPacientesWeb
    );

    server.on(
        "/marcar-revisado",
        HTTP_POST,
        marcarRevisadoWeb
    );

    server.on(
        "/iniciar-examen",
        HTTP_POST,
        iniciarExamenWeb
    );

    server.on(
        "/finalizar-examen",
        HTTP_POST,
        finalizarExamenWeb
    );

    server.begin();

    Serial.println(
        "WiFi local creado"
    );

    Serial.println(
        WiFi.softAPIP()
    );
}

// ================================================================
// ACTUALIZAR SERVIDOR
// ================================================================

void WebServer_update(
    int bpm,
    int spo2,
    float temperatura
)
{
    bpmActual = bpm;
    spo2Actual = spo2;
    tempActual = temperatura;

    server.handleClient();
}

// ================================================================
// AGREGAR MUESTRA ECG
// ================================================================

void WebServer_agregarMuestraECG(
    int ecg
)
{
    bufferECG[
        indiceEscrituraECG
    ] = ecg;

    indiceEscrituraECG++;

    if (indiceEscrituraECG >=
        TAM_BUFFER_ECG)
    {
        indiceEscrituraECG = 0;
    }

    if (cantidadMuestrasECG <
        TAM_BUFFER_ECG)
    {
        cantidadMuestrasECG++;
    }
}