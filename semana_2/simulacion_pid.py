"""
═══════════════════════════════════════════════════════════════
  CALCULADORA PID — Seguidor de Línea
  Basado en etapa3_pid.ino (Arduino UNO + L293D)
═══════════════════════════════════════════════════════════════
  Ingresa los parámetros físicos del robot y el script:
    1. Calcula la ganancia de la planta (K_planta)
    2. Sugiere Kp, Ki, Kd usando Ziegler-Nichols
    3. Simula la respuesta del sistema
    4. Muestra gráficas interactivas con sliders

  Requiere:  pip install numpy matplotlib
═══════════════════════════════════════════════════════════════
"""

import numpy as np
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
from matplotlib.widgets import Slider, Button

# ──────────────────────────────────────────────────────────────
#  PARÁMETROS FÍSICOS DEL ROBOT  ← EDITAR AQUÍ
# ──────────────────────────────────────────────────────────────

PARAMS = {
    # ── Motor ──────────────────────────────────────────────
    "rpm_max"          : 200,    # RPM del motor sin carga a plena tensión
    "voltaje_motor"    : 6.0,    # Voltios nominales del motor (V)
    "voltaje_bateria"  : 9.0,    # Voltaje real de la batería (V)

    # ── Ruedas ─────────────────────────────────────────────
    "diametro_rueda_cm": 6.5,    # Diámetro de la rueda (cm)
    "separacion_cm"    : 14.0,   # Distancia entre centros de ruedas (cm) = trocha

    # ── Sensores IR ────────────────────────────────────────
    "dist_sensores_cm" : 3.5,    # Distancia entre SL y SR (cm)
    "altura_sensor_cm" : 1.5,    # Altura del sensor sobre el piso (cm)
    "ancho_linea_cm"   : 2.0,    # Ancho de la línea negra (cm)

    # ── Robot (dimensiones físicas) ─────────────────────────
    "largo_cm"         : 15.3,   # Largo total del chasis (cm)
    "ancho_cm"         : 14.5,   # Ancho total del chasis (cm)
    "alto_cm"          : 10.0,    # Alto total del chasis (cm)
    "peso_g"           : 280,    # Peso total con batería (gramos)

    # ── Arduino / L293D ────────────────────────────────────
    "vel_base"         : 160,    # PWM base del .ino  (0-255)
    "vel_min"          : 60,     # PWM mínimo por motor
    "vel_max"          : 240,    # PWM máximo por motor
    "integral_max"     : 800,    # Anti-windup del .ino
    "dt_ms"            : 20,     # Período de muestreo (ms) — delay(20) en el .ino
    "adc_bits"         : 10,     # Resolución ADC del Arduino (10 bits = 0-1023)
    "adc_ref_v"        : 5.0,    # Tensión de referencia ADC (V)
}

# ──────────────────────────────────────────────────────────────
#  CÁLCULO DE PARÁMETROS DERIVADOS
# ──────────────────────────────────────────────────────────────

def calcular_planta(p):
    """
    Calcula las propiedades físicas del sistema a partir de los
    parámetros del robot. Retorna un diccionario con todas las
    variables derivadas y la ganancia de la planta K_planta.
    """
    DT  = p["dt_ms"] / 1000.0           # s
    ADC_MAX = (2 ** p["adc_bits"]) - 1  # 1023 para 10 bits

    # ── Velocidad lineal de las ruedas ──────────────────────
    # El L293D aplica el voltaje de la batería modulado por PWM.
    # A VEL_BASE/255 del duty cycle → velocidad proporcional a RPM
    factor_tension = min(p["voltaje_bateria"] / p["voltaje_motor"], 1.0)
    rpm_efectivo   = p["rpm_max"] * factor_tension

    # Velocidad lineal máxima (m/s)
    radio_m      = (p["diametro_rueda_cm"] / 2.0) / 100.0
    v_max_ms     = rpm_efectivo * 2 * np.pi * radio_m / 60.0

    # Velocidad a VEL_BASE PWM  (m/s)
    v_base_ms    = v_max_ms * (p["vel_base"] / 255.0)

    # Cambio de velocidad lineal por 1 unidad de corrección PWM (m/s por PWM)
    dv_por_pwm   = v_max_ms / 255.0

    # ── Cinemática diferencial ──────────────────────────────
    # La corrección PID u cambia la velocidad diferencial:
    #   ΔV = vIzq - vDer = 2 * u * dv_por_pwm
    # Velocidad angular resultante:
    #   ω = ΔV / L     [rad/s]
    L_m          = p["separacion_cm"] / 100.0
    omega_por_u  = 2.0 * dv_por_pwm / L_m   # rad/s por PWM de corrección

    # ── Sensibilidad del sensor ─────────────────────────────
    # El error ADC es proporcional al desplazamiento lateral.
    # Rango lateral visible por los dos sensores ≈ dist_sensores + ancho_linea
    rango_lateral_cm = p["dist_sensores_cm"] + p["ancho_linea_cm"]
    # ADC cambia en todo su rango dentro de esa zona
    sens_adc_por_cm  = ADC_MAX / rango_lateral_cm    # LSB / cm

    # ── Ganancia de la planta ───────────────────────────────
    # Relaciona corrección u (PWM) con cambio de error (ADC/ciclo)
    # error_ADC[k+1] - error_ADC[k] ≈ omega_por_u * u * v_base_ms * DT * sens
    K_planta = omega_por_u * v_base_ms * DT * sens_adc_por_cm

    # ── Período de muestreo y frecuencia ───────────────────
    fs = 1.0 / DT

    return {
        "DT"            : DT,
        "ADC_MAX"       : ADC_MAX,
        "rpm_efectivo"  : rpm_efectivo,
        "v_max_ms"      : v_max_ms,
        "v_base_ms"     : v_base_ms,
        "dv_por_pwm"    : dv_por_pwm,
        "omega_por_u"   : omega_por_u,
        "L_m"           : L_m,
        "sens_adc_cm"   : sens_adc_por_cm,
        "K_planta"      : K_planta,
        "fs"            : fs,
    }

# ──────────────────────────────────────────────────────────────
#  SINTONIZACIÓN ZIEGLER-NICHOLS
# ──────────────────────────────────────────────────────────────

def ziegler_nichols(K_planta, DT):
    """
    Estima Kp, Ki, Kd usando Ziegler-Nichols basado en la ganancia
    de la planta calculada de los parámetros físicos.

    Para un sistema con ganancia K e integrador implícito:
      Ku ≈ 1 / (pi/2 * K_planta)   (ganancia de oscilación)
      Tu ≈ 4 * DT                   (período mínimo detectable)
    """
    # Ganancia última estimada: la planta oscila cuando la corrección
    # retroalimenta con 180° de fase — aproximación para integrador puro
    Ku = 0.45 / K_planta if K_planta > 0 else 1.0
    Tu = 8.0 * DT          # estimación conservadora para 50 Hz

    # Tabla Ziegler-Nichols clásica
    kp_zn = 0.600 * Ku
    ti    = Tu / 1.200          # tiempo integral
    td    = Tu * 0.125          # tiempo derivativo
    ki_zn = kp_zn / ti
    kd_zn = kp_zn * td

    # Versión "sin sobreimpulso" (recomendada para seguidor de línea)
    kp_ns = 0.20 * Ku
    ki_ns = kp_ns / (Tu / 1.0)
    kd_ns = kp_ns * (Tu / 3.0)

    return {
        "Ku": Ku, "Tu": Tu,
        "zn" : (kp_zn, ki_zn, kd_zn),
        "ns" : (kp_ns, ki_ns, kd_ns),
    }

# ──────────────────────────────────────────────────────────────
#  SIMULACIÓN PID
# ──────────────────────────────────────────────────────────────

def perturbacion(t):
    """
    Simula una trayectoria con curvas.
    Modifica esta función para cambiar el perfil de la pista.
    """
    if t < 0.4:   return 0.0
    if t < 0.9:   return 350.0 * (t - 0.4) / 0.5    # curva derecha
    if t < 1.6:   return 350.0
    if t < 2.1:   return 350.0 - 700.0 * (t - 1.6) / 0.5   # curva izquierda
    if t < 2.5:   return -350.0
    return 0.0

def simular_pid(kp, ki, kd, planta, T_TOTAL=3.0):
    DT       = planta["DT"]
    K        = planta["K_planta"]
    ADC_MAX  = planta["ADC_MAX"]
    INT_MAX  = PARAMS["integral_max"] * DT
    VEL_BASE = PARAMS["vel_base"]
    VEL_MIN  = PARAMS["vel_min"]
    VEL_MAX  = PARAMS["vel_max"]

    n   = int(T_TOTAL / DT)
    t   = np.zeros(n);   pos = np.zeros(n);   linea = np.zeros(n)
    err = np.zeros(n);   vi  = np.zeros(n);   vd    = np.zeros(n)
    tp  = np.zeros(n);   ti_ = np.zeros(n);   td_   = np.zeros(n)

    pos_actual = 0.0;  integral = 0.0;  e_ant = 0.0

    for k in range(n):
        t[k]     = k * DT
        ref      = perturbacion(t[k])        # posición deseada
        e        = pos_actual - ref          # error

        integral += e * DT
        integral  = np.clip(integral, -INT_MAX, INT_MAX)
        deriv     = (e - e_ant) / DT if k > 0 else 0.0

        u = kp * e + ki * integral + kd * deriv

        v_izq = np.clip(VEL_BASE + u, VEL_MIN, VEL_MAX)
        v_der = np.clip(VEL_BASE - u, VEL_MIN, VEL_MAX)

        # Dinámica: corrección diferencial → cambio de posición
        delta_v    = v_izq - v_der
        pos_actual -= K * delta_v

        linea[k] = ref;  pos[k] = pos_actual;  err[k] = e
        vi[k]    = v_izq;  vd[k] = v_der
        tp[k] = kp * e;  ti_[k] = ki * integral;  td_[k] = kd * deriv
        e_ant = e

    return {"t": t, "pos": pos, "linea": linea, "error": err,
            "vi": vi, "vd": vd, "P": tp, "I": ti_, "D": td_}

def metricas(d):
    e = d["error"];  t = d["t"];  dt = PARAMS["dt_ms"] / 1000.0
    n = len(e)
    return {
        "IAE"   : np.sum(np.abs(e)) * dt,
        "ISE"   : np.sum(e**2) * dt,
        "ITAE"  : np.sum(t * np.abs(e)) * dt,
        "OS"    : np.max(np.abs(d["pos"] - d["linea"])),
        "ESS"   : np.mean(np.abs(e[int(0.8*n):])),
    }

# ──────────────────────────────────────────────────────────────
#  REPORTE EN CONSOLA
# ──────────────────────────────────────────────────────────────

def imprimir_reporte(planta, zn):
    ADC_RES = PARAMS["adc_ref_v"] / planta["ADC_MAX"] * 1000   # mV/LSB

    print("\n" + "═"*58)
    print("  CALCULADORA PID — Seguidor de Línea")
    print("═"*58)
    print("  PARÁMETROS FÍSICOS")
    print(f"  RPM máx (sin carga)   : {PARAMS['rpm_max']} RPM")
    print(f"  RPM efectivos (carga) : {planta['rpm_efectivo']:.1f} RPM")
    print(f"  Diámetro rueda        : {PARAMS['diametro_rueda_cm']} cm")
    print(f"  Separación de ruedas  : {PARAMS['separacion_cm']} cm")
    print(f"  Separación sensores   : {PARAMS['dist_sensores_cm']} cm")
    print(f"  Altura sensores       : {PARAMS['altura_sensor_cm']} cm")
    print(f"  Ancho línea           : {PARAMS['ancho_linea_cm']} cm")
    print(f"  Peso total            : {PARAMS['peso_g']} g")
    print("─"*58)
    print("  VELOCIDADES CALCULADAS")
    print(f"  v_max (rueda)         : {planta['v_max_ms']*100:.2f} cm/s")
    print(f"  v_base (VEL={PARAMS['vel_base']})      : {planta['v_base_ms']*100:.2f} cm/s")
    print(f"  Δv por 1 PWM          : {planta['dv_por_pwm']*1000:.4f} mm/s")
    print(f"  ω por 1 PWM correc.   : {planta['omega_por_u']:.6f} rad/s")
    print("─"*58)
    print("  SENSORES ADC")
    print(f"  Resolución ADC        : {ADC_RES:.2f} mV/LSB")
    print(f"  Sensibilidad          : {planta['sens_adc_cm']:.2f} LSB/cm")
    print(f"  Frecuencia muestreo   : {planta['fs']:.0f} Hz  (DT={PARAMS['dt_ms']}ms)")
    print("─"*58)
    print("  GANANCIA DE LA PLANTA")
    print(f"  K_planta              : {planta['K_planta']:.6f}  ADC/PWM/ciclo")
    print(f"  Ku (ganancia última)  : {zn['Ku']:.4f}")
    print(f"  Tu (período último)   : {zn['Tu']*1000:.1f} ms")
    print("─"*58)
    print("  GANANCIAS SUGERIDAS")
    print(f"  {'Método':<28} {'Kp':>8}  {'Ki':>8}  {'Kd':>8}")
    print(f"  {'─'*28} {'─'*8}  {'─'*8}  {'─'*8}")
    kp,ki,kd = zn["zn"]
    print(f"  {'Ziegler-Nichols PID':<28} {kp:>8.4f}  {ki:>8.5f}  {kd:>8.5f}")
    kp,ki,kd = zn["ns"]
    print(f"  {'Z-N sin sobreimpulso':<28} {kp:>8.4f}  {ki:>8.5f}  {kd:>8.5f}")
    print(f"  {'Valor actual en .ino':<28} {0.15:>8.4f}  {0.002:>8.5f}  {0.08:>8.5f}")
    print("═"*58 + "\n")

# ──────────────────────────────────────────────────────────────
#  CÓDIGO ARDUINO LISTO PARA COPIAR
# ──────────────────────────────────────────────────────────────

def imprimir_codigo_arduino(kp, ki, kd, planta, etiqueta=""):
    """
    Imprime el bloque #define exacto para pegar en etapa3_pid.ino.
    También sugiere el valor de UMBRAL basado en la sensibilidad del sensor.
    """
    VEL_BASE     = PARAMS["vel_base"]
    VEL_MIN      = PARAMS["vel_min"]
    VEL_MAX      = PARAMS["vel_max"]
    INT_MAX      = PARAMS["integral_max"]
    DT_MS        = PARAMS["dt_ms"]

    # UMBRAL sugerido: punto medio del ADC donde el sensor transiciona
    # Si el sensor lee ~0 sobre línea y ~1023 fuera → umbral = 512
    # Ajustar si el sensor tiene rango menor (medir con Serial.println)
    umbral_sugerido = int(planta["ADC_MAX"] * 0.40)   # 40% del rango = inicio de línea

    sep = "═" * 62
    print(f"\n{sep}")
    if etiqueta:
        print(f"  CÓDIGO ARDUINO — {etiqueta}")
    else:
        print( "  CÓDIGO ARDUINO — Copiar y pegar en etapa3_pid.ino")
    print(sep)
    print()
    print("  // ── Parámetros PID calculados por simulacion_pid.py ──")
    print(f"  #define KP           {kp:.4f}f")
    print(f"  #define KI           {ki:.5f}f")
    print(f"  #define KD           {kd:.4f}f")
    print()
    print("  // ── Velocidades ──────────────────────────────────────")
    print(f"  #define VEL_BASE     {VEL_BASE}      // PWM base ({planta['v_base_ms']*100:.1f} cm/s)")
    print(f"  #define VEL_MIN      {VEL_MIN}")
    print(f"  #define VEL_MAX      {VEL_MAX}")
    print(f"  #define INTEGRAL_MAX {INT_MAX}")
    print()
    print("  // ── Sensor umbral sugerido ───────────────────────────")
    print(f"  #define UMBRAL_LINEA {umbral_sugerido}    // ajustar con Serial.println(analogRead(SL))")
    print()
    print("  // ── Frecuencia de control ────────────────────────────")
    print(f"  // delay({DT_MS});   →   {1000//DT_MS} Hz")
    print()
    print("  // ── Diagnóstico: pegar en loop() para verificar ──────")
    print('  // Serial.print("SL:"); Serial.print(analogRead(A1));')
    print('  // Serial.print(" SR:"); Serial.println(analogRead(A0));')
    print()
    print(f"  // K_planta calculada = {planta['K_planta']:.6f} ADC/PWM/ciclo")
    print(f"  // v_base real        = {planta['v_base_ms']*100:.2f} cm/s")
    print(f"  // Separación ruedas  = {PARAMS['separacion_cm']} cm")
    print(f"  // Separación sensor  = {PARAMS['dist_sensores_cm']} cm")
    print(sep + "\n")

# ──────────────────────────────────────────────────────────────
#  INTERFAZ GRÁFICA INTERACTIVA
# ──────────────────────────────────────────────────────────────

def lanzar_interfaz(planta, kp0, ki0, kd0):
    fig = plt.figure(figsize=(16, 9))
    fig.patch.set_facecolor('#0d1117')
    gs  = gridspec.GridSpec(3, 2, figure=fig, hspace=0.50, wspace=0.32,
                            top=0.93, bottom=0.20)

    ax1 = fig.add_subplot(gs[0, :])
    ax2 = fig.add_subplot(gs[1, 0])
    ax3 = fig.add_subplot(gs[1, 1])
    ax4 = fig.add_subplot(gs[2, 0])
    ax5 = fig.add_subplot(gs[2, 1])

    BG    = '#0d1117';  GRID  = '#21262d';  TEXT  = '#c9d1d9'
    AZUL  = '#58a6ff';  NARJ  = '#f78166';  AMAR  = '#e3b341'
    VERD  = '#3fb950';  ROJO  = '#f85149';  PURP  = '#bc8cff'

    def ax_fmt(ax, titulo, xlabel='Tiempo (s)', ylabel=''):
        ax.set_facecolor(BG)
        ax.set_title(titulo, color=TEXT, fontsize=9, pad=5)
        ax.set_xlabel(xlabel, color=TEXT, fontsize=8)
        if ylabel: ax.set_ylabel(ylabel, color=TEXT, fontsize=8)
        ax.tick_params(colors=TEXT, labelsize=7)
        ax.grid(True, color=GRID, lw=0.6)
        for s in ax.spines.values(): s.set_edgecolor(GRID)

    def graficar(kp, ki, kd):
        d = simular_pid(kp, ki, kd, planta)
        m = metricas(d)
        t = d["t"]
        for ax in [ax1,ax2,ax3,ax4,ax5]: ax.cla()

        # Gráfica 1: posición
        ax1.plot(t, d["pos"],   color=AZUL, lw=2.0, label='Robot')
        ax1.plot(t, d["linea"], color=NARJ, lw=1.5, ls='--', label='Línea')
        ax1.fill_between(t, d["pos"], d["linea"], alpha=0.08, color=AZUL)
        ax_fmt(ax1, 'Posición del robot vs línea  (ADC)', ylabel='ADC')
        ax1.legend(fontsize=8, facecolor=BG, labelcolor=TEXT, loc='upper right')

        # Gráfica 2: error
        ax2.plot(t, d["error"], color=AMAR, lw=1.5)
        ax2.fill_between(t, d["error"], 0, alpha=0.12, color=AMAR)
        ax2.axhline(0, color=TEXT, lw=0.5, ls=':')
        ax_fmt(ax2, 'Error PID e(t)', ylabel='ADC')

        # Gráfica 3: velocidades
        ax3.plot(t, d["vi"], color=VERD, lw=1.5, label='Motor Izq')
        ax3.plot(t, d["vd"], color=ROJO, lw=1.5, label='Motor Der')
        ax3.axhline(PARAMS["vel_base"], color=TEXT, lw=0.5, ls=':', alpha=0.5)
        ax3.set_ylim(0, 260)
        ax_fmt(ax3, 'Velocidad PWM por motor', ylabel='PWM (0-255)')
        ax3.legend(fontsize=8, facecolor=BG, labelcolor=TEXT)

        # Gráfica 4: términos PID
        ax4.plot(t, d["P"], color=AZUL, lw=1.3, label='P')
        ax4.plot(t, d["I"], color=AMAR, lw=1.3, label='I')
        ax4.plot(t, d["D"], color=PURP, lw=1.3, label='D')
        ax4.axhline(0, color=TEXT, lw=0.4, ls=':')
        ax_fmt(ax4, 'Contribución de cada término PID', ylabel='u parcial')
        ax4.legend(fontsize=8, facecolor=BG, labelcolor=TEXT)

        # Gráfica 5: métricas + sugerencias
        ax5.set_facecolor(BG); ax5.axis('off')
        kp_zn, ki_zn, kd_zn = zn["zn"]
        kp_ns, ki_ns, kd_ns = zn["ns"]
        aviso = ""
        if m["ESS"] > 10:    aviso += "  ⚠ Aumentar Ki (error estacionario)\n"
        if m["OS"] > 300:    aviso += "  ⚠ Reducir Kp o aumentar Kd (overshoot)\n"
        if m["ITAE"] > 100:  aviso += "  ⚠ Aumentar Kp (respuesta lenta)\n"
        if not aviso:        aviso =  "  ✓ Respuesta aceptable\n"

        txt = (
            f"  Kp={kp:.4f}  Ki={ki:.5f}  Kd={kd:.4f}\n"
            f"  ─────────────────────────────────\n"
            f"  IAE   = {m['IAE']:>9.2f}   (↓ mejor)\n"
            f"  ISE   = {m['ISE']:>9.2f}   (↓ mejor)\n"
            f"  ITAE  = {m['ITAE']:>9.2f}   (↓ mejor)\n"
            f"  Overshoot = {m['OS']:>6.1f} ADC\n"
            f"  Error SS  = {m['ESS']:>6.2f} ADC\n"
            f"  ─────────────────────────────────\n"
            f"  Sugeridos (Z-N):\n"
            f"    Kp={kp_zn:.4f}  Ki={ki_zn:.5f}  Kd={kd_zn:.4f}\n"
            f"  Sin sobreimpulso:\n"
            f"    Kp={kp_ns:.4f}  Ki={ki_ns:.5f}  Kd={kd_ns:.4f}\n"
            f"  ─────────────────────────────────\n"
            f"{aviso}"
        )
        ax5.text(0.03, 0.97, txt, transform=ax5.transAxes, color=TEXT,
                 va='top', fontsize=8.5, fontfamily='monospace',
                 bbox=dict(facecolor='#161b22', edgecolor=GRID, pad=8, lw=0.8))
        ax_fmt(ax5, 'Métricas y sugerencias', xlabel='', ylabel='')

        fig.canvas.draw_idle()

    # ── Sliders ───────────────────────────────────────────
    plt.subplots_adjust(bottom=0.22)
    col_sl = '#161b22'
    ax_kp = plt.axes([0.17, 0.13, 0.61, 0.022], facecolor=col_sl)
    ax_ki = plt.axes([0.17, 0.09, 0.61, 0.022], facecolor=col_sl)
    ax_kd = plt.axes([0.17, 0.05, 0.61, 0.022], facecolor=col_sl)

    sl_kp = Slider(ax_kp, '', 0.0, 1.0,  valinit=kp0, color=AZUL)
    sl_ki = Slider(ax_ki, '', 0.0, 0.05, valinit=ki0, color=AMAR)
    sl_kd = Slider(ax_kd, '', 0.0, 0.8,  valinit=kd0, color=PURP)

    for sl in [sl_kp, sl_ki, sl_kd]:
        sl.valtext.set_visible(False)
        sl.ax.tick_params(labelbottom=False, bottom=False)

    # Labels externos — izquierda de cada barra, sin tocarla
    fig.text(0.09, 0.141, 'Kp', color=AZUL, fontsize=10,
             fontweight='bold', va='center', fontfamily='monospace')
    fig.text(0.09, 0.101, 'Ki', color=AMAR, fontsize=10,
             fontweight='bold', va='center', fontfamily='monospace')
    fig.text(0.09, 0.061, 'Kd', color=PURP, fontsize=10,
             fontweight='bold', va='center', fontfamily='monospace')

    def actualizar(_):
        graficar(sl_kp.val, sl_ki.val, sl_kd.val)

    sl_kp.on_changed(actualizar)
    sl_ki.on_changed(actualizar)
    sl_kd.on_changed(actualizar)

    ax_btn_r = plt.axes([0.80, 0.04, 0.08, 0.05], facecolor=col_sl)
    ax_btn_z = plt.axes([0.80, 0.10, 0.08, 0.05], facecolor=col_sl)
    ax_btn_a = plt.axes([0.80, 0.16, 0.08, 0.05], facecolor=col_sl)
    btn_r = Button(ax_btn_r, 'Reset',      color=col_sl, hovercolor='#21262d')
    btn_z = Button(ax_btn_z, 'Z-N',        color=col_sl, hovercolor='#21262d')
    btn_a = Button(ax_btn_a, '→ Arduino',  color='#1a3a1a', hovercolor='#2d5a2d')
    for b in [btn_r, btn_z]: b.label.set_color(TEXT)
    btn_a.label.set_color('#3fb950')

    def reset(_):
        sl_kp.reset(); sl_ki.reset(); sl_kd.reset()

    def aplicar_zn(_):
        kp_zn, ki_zn, kd_zn = zn["zn"]
        sl_kp.set_val(kp_zn)
        sl_ki.set_val(ki_zn)
        sl_kd.set_val(kd_zn)

    def exportar_arduino(_):
        imprimir_codigo_arduino(sl_kp.val, sl_ki.val, sl_kd.val, planta,
                                etiqueta="valores actuales de sliders")

    btn_r.on_clicked(reset)
    btn_z.on_clicked(aplicar_zn)
    btn_a.on_clicked(exportar_arduino)

    fig.suptitle(
        f'Simulador PID — Seguidor de Línea  |  '
        f'K_planta={planta["K_planta"]:.5f}  '
        f'v_base={planta["v_base_ms"]*100:.1f} cm/s  '
        f'fs={planta["fs"]:.0f} Hz',
        color=TEXT, fontsize=10, fontweight='bold')

    graficar(kp0, ki0, kd0)
    plt.show()

# ──────────────────────────────────────────────────────────────
#  COMPARACIÓN P vs PD vs PID
# ──────────────────────────────────────────────────────────────

def grafica_comparacion(planta, kp_base, ki_base, kd_base):
    configs = [
        ("Solo P\n(Ki=0, Kd=0)",  kp_base,  0.0,    0.0    ),
        ("PD\n(Ki=0)",            kp_base,  0.0,    kd_base),
        ("PID completo",          kp_base,  ki_base, kd_base),
    ]
    BG   = '#0d1117'; GRID = '#21262d'; TEXT = '#c9d1d9'
    AZUL = '#58a6ff'; NARJ = '#f78166'

    fig, axes = plt.subplots(1, 3, figsize=(15, 4.5))
    fig.patch.set_facecolor(BG)
    fig.suptitle('Comparación: P  vs  PD  vs  PID  (misma Kp)',
                 color=TEXT, fontsize=11, fontweight='bold')

    for ax, (nombre, kp, ki, kd) in zip(axes, configs):
        d = simular_pid(kp, ki, kd, planta)
        m = metricas(d)
        ax.set_facecolor(BG)
        ax.plot(d["t"], d["pos"],   color=AZUL, lw=2.0, label='Robot')
        ax.plot(d["t"], d["linea"], color=NARJ, lw=1.5, ls='--', label='Línea')
        ax.fill_between(d["t"], d["pos"], d["linea"], alpha=0.08, color=AZUL)
        ax.axhline(0, color=TEXT, lw=0.4, ls=':', alpha=0.4)
        ax.set_title(
            f"{nombre}\n"
            f"Kp={kp:.3f}  Ki={ki:.4f}  Kd={kd:.3f}\n"
            f"IAE={m['IAE']:.1f}  OS={m['OS']:.0f}  ESS={m['ESS']:.1f}",
            color=TEXT, fontsize=8)
        ax.set_xlabel('Tiempo (s)', color=TEXT, fontsize=8)
        ax.set_ylabel('Posición ADC', color=TEXT, fontsize=8)
        ax.tick_params(colors=TEXT, labelsize=7)
        ax.grid(True, color=GRID, lw=0.5)
        ax.legend(fontsize=7, facecolor=BG, labelcolor=TEXT)
        for s in ax.spines.values(): s.set_edgecolor(GRID)

    plt.tight_layout()
    plt.show()

# ──────────────────────────────────────────────────────────────
#  PUNTO DE ENTRADA
# ──────────────────────────────────────────────────────────────

if __name__ == "__main__":
    # 1. Calcular planta desde parámetros físicos
    planta = calcular_planta(PARAMS)

    # 2. Sintonización automática
    zn = ziegler_nichols(planta["K_planta"], planta["DT"])

    # 3. Reporte completo en consola
    imprimir_reporte(planta, zn)

    # 4. Ganancias de partida para la simulación
    kp0, ki0, kd0 = zn["ns"]   # empieza con Z-N sin sobreimpulso

    # 5. Código Arduino listo para pegar (ambos métodos Z-N)
    imprimir_codigo_arduino(*zn["ns"], planta, etiqueta="Z-N sin sobreimpulso (RECOMENDADO)")
    imprimir_codigo_arduino(*zn["zn"], planta, etiqueta="Z-N clásico")

    # 6. Gráfica de comparación P vs PD vs PID
    grafica_comparacion(planta, kp0, ki0, kd0)

    # 7. Interfaz interactiva con sliders
    #    Botón "→ Arduino" imprime el bloque #define con los valores actuales
    lanzar_interfaz(planta, kp0, ki0, kd0)
