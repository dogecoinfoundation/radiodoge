import { sveltekit } from '@sveltejs/kit/vite';
import tailwindcss from '@tailwindcss/vite';
import { defineConfig } from 'vite';

// The Tauri dev server host — must match tauri.conf.json devUrl
const host = process.env.TAURI_DEV_HOST;

export default defineConfig({
	plugins: [
		// TailwindCSS v4 Vite plugin (no tailwind.config.ts needed)
		tailwindcss(),
		// SvelteKit
		sveltekit()
	],

	// Prevent Vite from obscuring Rust errors in the terminal
	clearScreen: false,

	server: {
		port: 5173,
		strictPort: true,
		host: host || false,
		hmr: host
			? {
					protocol: 'ws',
					host,
					port: 5174
				}
			: undefined,
		watch: {
			// Exclude Tauri's Rust source from the Vite file watcher
			ignored: ['**/src-tauri/**']
		}
	},

	// Make Vite's build output compatible with Tauri's file serving
	build: {
		outDir: 'build',
		emptyOutDir: true
	}
});
