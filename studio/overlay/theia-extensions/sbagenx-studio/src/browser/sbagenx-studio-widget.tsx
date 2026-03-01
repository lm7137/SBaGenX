/********************************************************************************
 * Copyright (C) 2026 SBaGenX contributors.
 *
 * This program and the accompanying materials are made available under the
 * terms of the MIT License, which is available in the project root.
 *
 * SPDX-License-Identifier: MIT
 ********************************************************************************/

import * as React from 'react';
import { injectable, inject, postConstruct } from '@theia/core/shared/inversify';
import { ReactWidget } from '@theia/core/lib/browser/widgets/react-widget';
import { CommandRegistry } from '@theia/core/lib/common/command';
import { nls } from '@theia/core/lib/common/nls';
import { SbagenxStudioInspectionResult } from '../common/sbagenx-studio-engine-protocol';
import { SbagenxStudioModel } from './sbagenx-studio-model';
import { SbagenxStudioCommands } from './sbagenx-studio-contribution';

@injectable()
export class SbagenxStudioWidget extends ReactWidget {

    static readonly ID = 'sbagenx-studio:view';
    static readonly LABEL = 'SBaGenX Studio';

    @inject(SbagenxStudioModel)
    protected readonly model: SbagenxStudioModel;

    @inject(CommandRegistry)
    protected readonly commands: CommandRegistry;

    @postConstruct()
    protected init(): void {
        this.id = SbagenxStudioWidget.ID;
        this.title.label = SbagenxStudioWidget.LABEL;
        this.title.caption = SbagenxStudioWidget.LABEL;
        this.title.iconClass = 'codicon codicon-file-code';
        this.title.closable = true;
        this.addClass('sbagenx-studio-widget');
        this.toDispose.push(this.model.onDidChange(() => this.update()));
        this.update();
    }

    protected render(): React.ReactNode {
        const summary = this.model.summary;
        const validation = this.model.validation;
        const programs = summary?.referencedPrograms.length ? summary.referencedPrograms.join(', ') : 'none';

        return <div className='sbagenx-studio-shell'>
            <header className='sbagenx-studio-header'>
                <div>
                    <p className='sbagenx-studio-kicker'>SBaGenX IDE</p>
                    <h2>Studio</h2>
                    <p className='sbagenx-studio-subtitle'>Open `.sbg` or `.sbgf` files, inspect their structure, and validate `.sbg` timing files directly through `sbagenxlib`.</p>
                </div>
                <div className='sbagenx-studio-actions'>
                    <button className='theia-button main' onClick={() => this.openSession()}>Open Session File</button>
                    <button className='theia-button secondary' onClick={() => this.refresh()} disabled={!summary || this.model.busy}>Refresh</button>
                </div>
            </header>

            {this.model.busy && <section className='sbagenx-studio-banner info'>Reading session file and querying engine validation...</section>}
            {this.model.error && <section className='sbagenx-studio-banner error'>{this.model.error}</section>}

            {!summary && !this.model.busy && !this.model.error && <section className='sbagenx-studio-empty'>
                <h3>No SBaGenX session selected</h3>
                <p>Use <strong>SBaGenX: Open Session File</strong> from the command palette or open an `.sbg` / `.sbgf` file in the editor. The Studio view will follow the active editor.</p>
            </section>}

            {summary && <div className='sbagenx-studio-content'>
                <section className='sbagenx-studio-card'>
                    <h3>{summary.uri.path.base}</h3>
                    <p className='sbagenx-studio-path'>{summary.uri.toString()}</p>
                    <div className='sbagenx-studio-grid'>
                        <Metric label='Detected kind' value={summary.detectedMode} />
                        <Metric label='File type' value={summary.fileType.toUpperCase()} />
                        <Metric label='Bytes' value={summary.byteSize.toLocaleString()} />
                        <Metric label='Program references' value={programs} />
                    </div>
                </section>

                <section className='sbagenx-studio-card'>
                    <h3>Engine validation</h3>
                    {!validation && <p className='sbagenx-studio-note'>No engine result yet.</p>}
                    {validation && <>
                        <div className='sbagenx-studio-grid'>
                            <Metric label='Status' value={<StatusPill result={validation} />} />
                            <Metric label='Backend' value={validation.backend} />
                            <Metric label='Source mode' value={validation.sourceMode ?? 'n/a'} />
                            <Metric label='API version' value={validation.apiVersion ?? 'n/a'} />
                            <Metric label='Voice lanes' value={validation.voiceCount ?? 'n/a'} />
                            <Metric label='Keyframes' value={validation.keyframeCount ?? 'n/a'} />
                            <Metric label='Duration (sec)' value={validation.durationSec !== undefined ? validation.durationSec.toFixed(2) : 'n/a'} />
                            <Metric label='Looping' value={formatBool(validation.looping)} />
                            <Metric label='Mix amp control' value={formatBool(validation.hasMixAmpControl)} />
                            <Metric label='Mix effects' value={formatBool(validation.hasMixEffects)} />
                        </div>
                        {validation.message && <p className='sbagenx-studio-note'>{validation.message}</p>}
                        {validation.version && <p className='sbagenx-studio-note dim'>sbagenxlib {validation.version}</p>}
                    </>}
                </section>

                <section className='sbagenx-studio-card'>
                    <h3>Structure</h3>
                    <div className='sbagenx-studio-grid'>
                        <Metric label='Total lines' value={summary.totalLines} />
                        <Metric label='Non-empty lines' value={summary.nonEmptyLines} />
                        <Metric label='Comment lines' value={summary.commentLines} />
                        <Metric label='Timing entries' value={summary.timingEntries} />
                        <Metric label='Named definitions' value={summary.namedDefinitions} />
                        <Metric label='Block definitions' value={summary.blockDefinitions} />
                        <Metric label='Parameter definitions' value={summary.parameterDefinitions} />
                        <Metric label='Solve definitions' value={summary.solveDefinitions} />
                    </div>
                </section>

                <section className='sbagenx-studio-card'>
                    <h3>Signals</h3>
                    <ul className='sbagenx-studio-flags'>
                        <li className={summary.hasMixControls ? 'present' : 'absent'}>
                            {summary.hasMixControls ? 'Contains mix-side controls or effects' : 'No mix-side controls detected'}
                        </li>
                        <li className={summary.referencedPrograms.length ? 'present' : 'absent'}>
                            {summary.referencedPrograms.length ? `References built-in programs: ${programs}` : 'No built-in program wrapper detected'}
                        </li>
                    </ul>
                </section>

                <section className='sbagenx-studio-card'>
                    <h3>Preview</h3>
                    <pre className='sbagenx-studio-preview'>{summary.previewLines.length ? summary.previewLines.join('\n') : nls.localizeByDefault('No significant lines found.')}</pre>
                </section>
            </div>}
        </div>;
    }

    protected openSession(): void {
        this.commands.executeCommand(SbagenxStudioCommands.OPEN_SESSION.id).catch(() => undefined);
    }

    protected refresh(): void {
        const uri = this.model.summary?.uri;
        if (uri) {
            this.model.load(uri).catch(() => undefined);
        }
    }
}

function Metric(props: { label: string; value: React.ReactNode }): React.ReactElement {
    return <div className='sbagenx-studio-metric'>
        <span className='sbagenx-studio-metric-label'>{props.label}</span>
        <strong className='sbagenx-studio-metric-value'>{props.value}</strong>
    </div>;
}

function StatusPill(props: { result: SbagenxStudioInspectionResult }): React.ReactElement {
    return <span className={`sbagenx-studio-status ${props.result.status}`}>{props.result.status}</span>;
}

function formatBool(value: boolean | undefined): string {
    if (value === undefined) {
        return 'n/a';
    }
    return value ? 'yes' : 'no';
}
