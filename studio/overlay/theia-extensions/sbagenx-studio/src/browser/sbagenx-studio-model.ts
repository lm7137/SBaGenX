/********************************************************************************
 * Copyright (C) 2026 SBaGenX contributors.
 *
 * This program and the accompanying materials are made available under the
 * terms of the MIT License, which is available in the project root.
 *
 * SPDX-License-Identifier: MIT
 ********************************************************************************/

import URI from '@theia/core/lib/common/uri';
import { Emitter, Event } from '@theia/core/lib/common';
import { injectable, inject } from '@theia/core/shared/inversify';
import { FileService } from '@theia/filesystem/lib/browser/file-service';
import { SbagenxStudioEngineService, SbagenxStudioInspectionResult } from '../common/sbagenx-studio-engine-protocol';

export interface SbagenxStudioSummary {
    uri: URI;
    fileType: 'sbg' | 'sbgf';
    detectedMode: string;
    byteSize: number;
    totalLines: number;
    nonEmptyLines: number;
    commentLines: number;
    blockDefinitions: number;
    namedDefinitions: number;
    timingEntries: number;
    parameterDefinitions: number;
    solveDefinitions: number;
    hasMixControls: boolean;
    referencedPrograms: string[];
    previewLines: string[];
}

const COMMENT_PREFIXES = ['#', ';', '//'];
const TIMING_RE = /^(?:NOW(?:\+\d{1,2}:\d{2}(?::\d{2})?)?|\+\d{1,2}:\d{2}(?::\d{2})?|\d{1,2}:\d{2}(?::\d{2})?)(?:\s|$)/i;
const NAMED_RE = /^[A-Za-z_][\w-]*:\s*(?!\{)/;
const BLOCK_RE = /^[A-Za-z_][\w-]*:\s*\{$/;
const PROGRAM_RE = /(?:^|\s)-p\s+(drop|sigmoid|slide|curve)\b/ig;
const MIX_RE = /\bmix(?:\/[0-9.]+|spin\b|pulse\b|beat\b|am\b)/i;

@injectable()
export class SbagenxStudioModel {

    @inject(FileService)
    protected readonly fileService: FileService;

    @inject(SbagenxStudioEngineService)
    protected readonly engineService: SbagenxStudioEngineService;

    protected readonly onDidChangeEmitter = new Emitter<void>();
    readonly onDidChange: Event<void> = this.onDidChangeEmitter.event;

    protected _busy = false;
    protected _summary: SbagenxStudioSummary | undefined;
    protected _error: string | undefined;
    protected _validation: SbagenxStudioInspectionResult | undefined;
    protected requestSeq = 0;

    get busy(): boolean {
        return this._busy;
    }

    get summary(): SbagenxStudioSummary | undefined {
        return this._summary;
    }

    get error(): string | undefined {
        return this._error;
    }

    get validation(): SbagenxStudioInspectionResult | undefined {
        return this._validation;
    }

    canHandle(uri: URI | undefined): uri is URI {
        if (!uri) {
            return false;
        }
        const ext = uri.path.ext.toLowerCase();
        return ext === '.sbg' || ext === '.sbgf';
    }

    async load(uri: URI, textOverride?: string): Promise<void> {
        const requestId = ++this.requestSeq;
        if (!this.canHandle(uri)) {
            this.clear();
            return;
        }
        this._busy = true;
        this._error = undefined;
        this._validation = undefined;
        this.onDidChangeEmitter.fire();
        try {
            const text = textOverride ?? (await this.fileService.read(uri)).value.toString();
            const fileType = uri.path.ext.toLowerCase() === '.sbgf' ? 'sbgf' : 'sbg';
            const summary = this.summarize(uri, text, text.length);
            const validation = await this.engineService.inspectText(fileType, text, uri.path.base);
            if (requestId !== this.requestSeq) {
                return;
            }
            this._summary = summary;
            this._validation = validation;
        } catch (error) {
            if (requestId !== this.requestSeq) {
                return;
            }
            this._validation = undefined;
            this._error = error instanceof Error ? error.message : String(error);
        } finally {
            if (requestId !== this.requestSeq) {
                return;
            }
            this._busy = false;
            this.onDidChangeEmitter.fire();
        }
    }

    clear(): void {
        this._busy = false;
        this._summary = undefined;
        this._error = undefined;
        this._validation = undefined;
        this.onDidChangeEmitter.fire();
    }

    protected summarize(uri: URI, text: string, byteSize: number): SbagenxStudioSummary {
        const lines = text.split(/\r?\n/);
        const trimmedLines = lines.map(line => line.trim());
        const nonEmpty = trimmedLines.filter(Boolean);
        const commentLines = nonEmpty.filter(line => COMMENT_PREFIXES.some(prefix => line.startsWith(prefix)));
        const significant = nonEmpty.filter(line => !COMMENT_PREFIXES.some(prefix => line.startsWith(prefix)));
        const fileType = uri.path.ext.toLowerCase() === '.sbgf' ? 'sbgf' : 'sbg';
        const namedDefinitions = significant.filter(line => NAMED_RE.test(line)).length;
        const blockDefinitions = significant.filter(line => BLOCK_RE.test(line)).length;
        const timingEntries = significant.filter(line => TIMING_RE.test(line)).length;
        const parameterDefinitions = significant.filter(line => /^param\s+/i.test(line)).length;
        const solveDefinitions = significant.filter(line => /^solve\s+/i.test(line)).length;
        const referencedPrograms = Array.from(new Set([...significant.join('\n').matchAll(PROGRAM_RE)].map(match => match[1].toLowerCase())));
        const hasMixControls = significant.some(line => MIX_RE.test(line));
        const detectedMode = this.detectMode(fileType, significant, referencedPrograms);

        return {
            uri,
            fileType,
            detectedMode,
            byteSize,
            totalLines: lines.length,
            nonEmptyLines: nonEmpty.length,
            commentLines: commentLines.length,
            blockDefinitions,
            namedDefinitions,
            timingEntries,
            parameterDefinitions,
            solveDefinitions,
            hasMixControls,
            referencedPrograms,
            previewLines: significant.slice(0, 6)
        };
    }

    protected detectMode(fileType: 'sbg' | 'sbgf', significant: string[], programs: string[]): string {
        if (fileType === 'sbgf') {
            return 'Function file';
        }
        if (programs.length > 0 && significant.every(line => line.startsWith('-') || TIMING_RE.test(line))) {
            return 'Program wrapper / session launcher';
        }
        if (significant.some(line => TIMING_RE.test(line))) {
            return 'Timed sequence file';
        }
        return 'Named-tone / support file';
    }
}
