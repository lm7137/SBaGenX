/********************************************************************************
 * Copyright (C) 2026 SBaGenX contributors.
 *
 * This program and the accompanying materials are made available under the
 * terms of the MIT License, which is available in the project root.
 *
 * SPDX-License-Identifier: MIT
 ********************************************************************************/

import * as fs from 'fs';
import * as path from 'path';
import { execFile } from 'child_process';
import { injectable } from '@theia/core/shared/inversify';
import { SbagenxStudioEngineService, SbagenxStudioInspectionResult } from '../common/sbagenx-studio-engine-protocol';

@injectable()
export class SbagenxStudioEngineServiceImpl implements SbagenxStudioEngineService {

    protected buildPromise: Promise<string> | undefined;

    async inspectFile(filePath: string): Promise<SbagenxStudioInspectionResult> {
        const enginePath = await this.ensureEngineBinary();
        const stdout = await this.execFile(enginePath, [filePath]);
        const result = JSON.parse(stdout) as SbagenxStudioInspectionResult;
        return result;
    }

    protected async ensureEngineBinary(): Promise<string> {
        if (!this.buildPromise) {
            this.buildPromise = this.buildEngineBinary();
        }
        return this.buildPromise;
    }

    protected async buildEngineBinary(): Promise<string> {
        const repoRoot = this.findRepoRoot(__dirname);
        const scriptPath = path.join(repoRoot, 'studio', 'build-sbagenx-studio-engine.sh');
        const binaryPath = path.join(repoRoot, 'studio', 'bin', 'sbagenx-studio-engine');
        const sources = [
            path.join(repoRoot, 'studio', 'tools', 'sbagenx-studio-engine.c'),
            path.join(repoRoot, 'sbagenxlib.c'),
            path.join(repoRoot, 'sbagenxlib.h'),
            scriptPath
        ];
        if (!fs.existsSync(binaryPath) || this.isStale(binaryPath, sources)) {
            await this.execFile(scriptPath, [], { cwd: repoRoot });
        }
        return binaryPath;
    }

    protected isStale(binaryPath: string, sources: string[]): boolean {
        const binaryStat = fs.statSync(binaryPath);
        return sources.some(source => fs.existsSync(source) && fs.statSync(source).mtimeMs > binaryStat.mtimeMs);
    }

    protected findRepoRoot(startDir: string): string {
        let current = path.resolve(startDir);
        while (true) {
            if (fs.existsSync(path.join(current, 'sbagenxlib.c')) && fs.existsSync(path.join(current, 'studio'))) {
                return current;
            }
            const parent = path.dirname(current);
            if (parent === current) {
                throw new Error('Unable to locate SBaGenX repository root for Studio engine helper');
            }
            current = parent;
        }
    }

    protected execFile(command: string, args: string[], options?: { cwd?: string }): Promise<string> {
        return new Promise((resolve, reject) => {
            execFile(command, args, { cwd: options?.cwd, maxBuffer: 4 * 1024 * 1024 }, (error, stdout, stderr) => {
                if (error) {
                    reject(new Error(stderr.trim() || error.message));
                    return;
                }
                resolve(stdout.trim());
            });
        });
    }
}
