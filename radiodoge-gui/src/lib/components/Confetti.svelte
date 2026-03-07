<script lang="ts">
  /**
   * Confetti explosion component — triggers on successful Dogecoin transactions!
   *
   * Usage:
   *   <Confetti bind:this={confetti} />
   *   confetti.trigger(); // call this on tx success
   */

  let canvas: HTMLCanvasElement;
  let animationId: number | null = null;

  interface Particle {
    x: number;
    y: number;
    vx: number;
    vy: number;
    rotation: number;
    rotationSpeed: number;
    color: string;
    size: number;
    opacity: number;
    shape: 'rect' | 'circle';
  }

  const COLORS = [
    '#F5C518', // Doge yellow
    '#FF8C00', // Doge orange
    '#FFFFFF', // white
    '#00FF88', // neon green
    '#FFD740', // light yellow
    '#FFA333', // light orange
  ];

  let particles: Particle[] = [];
  let visible = false;

  /** Trigger the confetti explosion from the center of the screen. */
  export function trigger() {
    if (!canvas) return;

    const w = window.innerWidth;
    const h = window.innerHeight;

    // Create 200 particles
    particles = Array.from({ length: 200 }, () => {
      const angle = Math.random() * Math.PI * 2;
      const speed = 4 + Math.random() * 10;
      return {
        x: w / 2 + (Math.random() - 0.5) * 100,
        y: h / 2 + (Math.random() - 0.5) * 100,
        vx: Math.cos(angle) * speed,
        vy: Math.sin(angle) * speed - 4, // initial upward bias
        rotation: Math.random() * 360,
        rotationSpeed: (Math.random() - 0.5) * 10,
        color: COLORS[Math.floor(Math.random() * COLORS.length)],
        size: 6 + Math.random() * 10,
        opacity: 1,
        shape: Math.random() > 0.5 ? 'rect' : 'circle',
      };
    });

    visible = true;

    // Cancel any running animation
    if (animationId !== null) {
      cancelAnimationFrame(animationId);
    }

    animate();
  }

  function animate() {
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    canvas.width = window.innerWidth;
    canvas.height = window.innerHeight;

    ctx.clearRect(0, 0, canvas.width, canvas.height);

    let alive = false;

    for (const p of particles) {
      if (p.opacity <= 0) continue;
      alive = true;

      // Physics
      p.vy += 0.35;    // gravity
      p.vx *= 0.99;    // air resistance
      p.x += p.vx;
      p.y += p.vy;
      p.rotation += p.rotationSpeed;
      p.opacity -= 0.012;

      // Draw
      ctx.save();
      ctx.globalAlpha = Math.max(0, p.opacity);
      ctx.fillStyle = p.color;
      ctx.translate(p.x, p.y);
      ctx.rotate((p.rotation * Math.PI) / 180);

      if (p.shape === 'circle') {
        ctx.beginPath();
        ctx.arc(0, 0, p.size / 2, 0, Math.PI * 2);
        ctx.fill();
      } else {
        ctx.fillRect(-p.size / 2, -p.size / 4, p.size, p.size / 2);
      }

      ctx.restore();
    }

    if (alive) {
      animationId = requestAnimationFrame(animate);
    } else {
      visible = false;
      animationId = null;
    }
  }
</script>

<!-- Full-viewport overlay canvas, pointer-events: none so clicks pass through -->
{#if visible}
  <canvas
    bind:this={canvas}
    style="
      position: fixed;
      top: 0;
      left: 0;
      width: 100vw;
      height: 100vh;
      pointer-events: none;
      z-index: 9999;
    "
  ></canvas>
{/if}

<!-- Hidden canvas for pre-initialization -->
{#if !visible}
  <canvas bind:this={canvas} style="display:none"></canvas>
{/if}
