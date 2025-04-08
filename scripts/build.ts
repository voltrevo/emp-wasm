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

  // We need to fix this in the actual file rather than combining it with
  // `getEmscriptenCode` because the file itself is used when loading in NodeJS.
  await fixEmscriptenCode(gitRoot);

  await fs.copyFile(
    join(gitRoot, 'dist/build/jslib.js'),
    join(gitRoot, 'build/jslib.js'),
  );

  const workerCode = [
    await getEmscriptenCode(),
    await getAppendWorkerCode(),
  ].join('\n\n');

  let workerCodeJs = await fs.readFile(
    join(gitRoot, 'dist/src/ts/workerCode.js'),
    'utf-8',
  );

  workerCodeJs = workerCodeJs.replace(
    `'<<WORKER_CODE>>'`,
    JSON.stringify(workerCode),
  );

  await fs.writeFile(
    join(gitRoot, 'dist/src/ts/workerCode.js'),
    workerCodeJs,
    'utf-8',
  );
}

async function getEmscriptenCode() {
  const gitRoot = await getGitRoot();

  return await fs.readFile(
    join(gitRoot, 'build/jslib.js'),
    'utf-8',
  );
}

async function getAppendWorkerCode() {
  const gitRoot = await getGitRoot();

  const code = (await fs.readFile(
    join(gitRoot, 'dist/src/ts/appendWorker.js'),
    'utf-8',
  )).trim();

  // remove the last two lines
  let i = code.lastIndexOf('\n');
  i = code.lastIndexOf('\n', i - 1);

  return code.substring(0, i);
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

async function fixEmscriptenCode(gitRoot: string) {
  const path = join(gitRoot, 'dist/build/jslib.js');
  let content = await fs.readFile(path, 'utf-8');

  content = replace(
    content,
    ['function intArrayFromBase64(s) { if (typeof ENVIRONMENT_IS_NODE'],
    // Emscripten falsely detects deno as node here and only uses it for a
    // small performance optimization that doesn't work in deno.
    'function intArrayFromBase64(s) { if (false && typeof ENVIRONMENT_IS_NODE',
  );

  content = replace(
    content,
    ['import.meta.url'],
    // Emscripten uses the 'data:' prefix as a switch to realise that it can't
    // support certain things. We don't need those things, so we can just make
    // it look like that, and that fixes deno.
    '"data:workaround"',
  );

  content = replace(
    content,
    ['var fs = require("fs")', "var fs = require('fs')"],
    // This doesn't really affect behavior, but it fixes a nextjs issue where
    // it analyzes the require statically and fails even when the code works as
    // a whole.
    'var fs=(()=>{try{return require("fs")}catch(e){throw e}})();'
  );

  // For deno compatibility
  content = replace(
    content,
    ['import("module")', "import('module')"],
    'import("node:module")',
  );

  await fs.writeFile(path, content);
}

function replace(content: string, searches: string[], replace: string) {
  for (const search of searches) {
    const parts = content.split(search);

    if (parts.length === 1) {
      continue;
    }

    return parts.join(replace);
  }

  throw new Error(`Search strings not found in file: ${JSON.stringify(searches)}`)
}

build().catch(console.error);
