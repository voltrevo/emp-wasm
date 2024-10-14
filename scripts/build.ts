import { spawn } from 'child_process';
import { promises as fs } from 'fs';
import { join } from 'path';
import { exec } from 'child_process';

// Helper function to find the git repository root
async function getGitRoot(): Promise<string> {
  return new Promise((resolve, reject) => {
    exec('git rev-parse --show-toplevel', (error, stdout, stderr) => {
      if (error) {
        reject(new Error('Failed to find git root: ' + stderr));
      } else {
        resolve(stdout.trim());
      }
    });
  });
}

async function build() {
  const gitRoot = await getGitRoot();

  await shell('rm', ['-rf', 'dist', 'build'], gitRoot);

  const mbedtlsPath = join(gitRoot, 'external/mbedtls');
  
  // Check if ../external/mbedtls exists asynchronously
  try {
    await fs.access(mbedtlsPath);
    console.log('mbedtls directory exists.');
  } catch {
    console.log('mbedtls not found, running build_mbedtls.sh');
    await shell('./scripts/build_mbedtls.sh', [], gitRoot);
  }
  
  // Run ./build_wasm.sh
  console.log('Running build_wasm.sh');
  await shell('./scripts/build_wasm.sh', [], gitRoot);

  await shell('tsc', [], gitRoot);
}

async function shell(cmd: string, args: string[], cwd: string): Promise<void> {
  return new Promise((resolve, reject) => {
    const process = spawn(cmd, args, { stdio: 'inherit', cwd });

    process.on('close', (code) => {
      if (code !== 0) {
        reject(new Error(`Process exited with status code ${code}`));
      } else {
        resolve();
      }
    });
  });
}

build().catch(console.error);
