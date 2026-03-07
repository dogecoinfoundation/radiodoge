import adapter from '@sveltejs/adapter-static';
import { vitePreprocess } from '@sveltejs/vite-plugin-svelte';

/** @type {import('@sveltejs/kit').Config} */
const config = {
	preprocess: vitePreprocess(),
	kit: {
		// Static adapter for Tauri — outputs to `build/` directory
		// Tauri serves the files from disk, not a server.
		adapter: adapter({
			// Single-page app fallback so client-side routing works
			fallback: 'index.html'
		}),
		// Alias for clean imports
		alias: {
			$lib: './src/lib'
		}
	}
};

export default config;
