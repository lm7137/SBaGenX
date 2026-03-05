/********************************************************************************
 * Copyright (C) 2026 SBaGenX contributors.
 *
 * This program and the accompanying materials are made available under the
 * terms of the MIT License, which is available in the project root.
 *
 * SPDX-License-Identifier: MIT
 ********************************************************************************/

import URI from '@theia/core/lib/common/uri';
import { CommonMenus } from '@theia/core/lib/browser/common-frontend-contribution';
import { AbstractViewContribution } from '@theia/core/lib/browser/shell/view-contribution';
import { FrontendApplication } from '@theia/core/lib/browser/frontend-application';
import { FrontendApplicationContribution } from '@theia/core/lib/browser/frontend-application-contribution';
import { Command, CommandRegistry } from '@theia/core/lib/common/command';
import { MenuModelRegistry } from '@theia/core/lib/common/menu';
import { DisposableCollection } from '@theia/core/lib/common/disposable';
import { inject, injectable } from '@theia/core/shared/inversify';
import { DiagnosticSeverity } from '@theia/core/shared/vscode-languageserver-protocol';
import { FileDialogService } from '@theia/filesystem/lib/browser/file-dialog/file-dialog-service';
import { EditorManager } from '@theia/editor/lib/browser/editor-manager';
import { MessageService } from '@theia/core/lib/common/message-service';
import { ProblemManager } from '@theia/markers/lib/browser/problem/problem-manager';
import { SbagenxStudioModel } from './sbagenx-studio-model';
import { SbagenxStudioWidget } from './sbagenx-studio-widget';

export namespace SbagenxStudioCommands {
    export const CATEGORY = 'SBaGenX Studio';
    export const OPEN_SESSION: Command = {
        id: 'sbagenx-studio:open-session',
        label: 'Open Session File...',
        category: CATEGORY
    };
}

@injectable()
export class SbagenxStudioContribution extends AbstractViewContribution<SbagenxStudioWidget> implements FrontendApplicationContribution {
    protected readonly toDisposeOnEditorChange = new DisposableCollection();
    protected contentValidationTimer: number | undefined;
    protected readonly markerOwner = 'sbagenx-studio';
    protected lastMarkerUri: URI | undefined;


    @inject(FileDialogService)
    protected readonly fileDialogService: FileDialogService;

    @inject(EditorManager)
    protected readonly editorManager: EditorManager;

    @inject(MessageService)
    protected readonly messageService: MessageService;

    @inject(ProblemManager)
    protected readonly problemManager: ProblemManager;

    @inject(SbagenxStudioModel)
    protected readonly model: SbagenxStudioModel;

    constructor() {
        super({
            widgetId: SbagenxStudioWidget.ID,
            widgetName: SbagenxStudioWidget.LABEL,
            defaultWidgetOptions: {
                area: 'right'
            },
            toggleCommandId: 'sbagenx-studio:toggle',
            toggleKeybinding: 'ctrlcmd+alt+s'
        });
    }

    registerCommands(commands: CommandRegistry): void {
        super.registerCommands(commands);
        commands.registerCommand(SbagenxStudioCommands.OPEN_SESSION, {
            execute: async () => this.openSessionFromDialog()
        });
    }

    registerMenus(menus: MenuModelRegistry): void {
        super.registerMenus(menus);
        menus.registerMenuAction(CommonMenus.FILE_OPEN, {
            commandId: SbagenxStudioCommands.OPEN_SESSION.id,
            label: 'Open SBaGenX Session...'
        });
    }

    onStart(_app: FrontendApplication): void {
        this.editorManager.onCurrentEditorChanged(editor => {
            this.bindCurrentEditor(editor);
        });
        this.model.onDidChange(() => this.syncMarkers());

        this.bindCurrentEditor(this.editorManager.currentEditor);
    }

    protected async openSessionFromDialog(): Promise<void> {
        const selection = await this.fileDialogService.showOpenDialog({
            title: 'Open SBaGenX Session',
            openLabel: 'Open Session',
            canSelectFiles: true,
            canSelectFolders: false,
            filters: {
                'SBaGenX session files': ['sbg', 'sbgf'],
                'SBG sequence files': ['sbg'],
                'SBGF function files': ['sbgf']
            }
        });
        const uri = Array.isArray(selection) ? selection[0] : selection;
        if (!uri) {
            return;
        }
        await this.openSession(uri);
    }

    protected async openSession(uri: URI): Promise<void> {
        if (!this.model.canHandle(uri)) {
            this.messageService.warn('SBaGenX Studio currently supports .sbg and .sbgf files.');
            return;
        }
        await this.editorManager.open(uri, { mode: 'activate' });
        const currentEditor = this.editorManager.currentEditor;
        const currentUri = currentEditor?.getResourceUri();
        if (currentUri && currentUri.toString() === uri.toString()) {
            await this.model.load(uri, currentEditor?.editor.document.getText());
        } else {
            await this.model.load(uri);
        }
        await this.openView({ activate: true, reveal: true });
    }

    protected reportLoadFailure(error: unknown): void {
        const message = error instanceof Error ? error.message : String(error);
        this.messageService.error(`Failed to inspect SBaGenX session: ${message}`);
    }

    protected bindCurrentEditor(editor: EditorManager['currentEditor']): void {
        this.toDisposeOnEditorChange.dispose();
        if (this.contentValidationTimer !== undefined) {
            window.clearTimeout(this.contentValidationTimer);
            this.contentValidationTimer = undefined;
        }
        const uri = editor?.getResourceUri();
        if (!this.model.canHandle(uri)) {
            this.model.clear();
            this.syncMarkers();
            return;
        }
        this.model.load(uri, editor?.editor.document.getText()).catch(error => this.reportLoadFailure(error));
        const listener = editor?.editor.onDocumentContentChanged(() => {
            if (this.contentValidationTimer !== undefined) {
                window.clearTimeout(this.contentValidationTimer);
            }
            this.contentValidationTimer = window.setTimeout(() => {
                const currentEditor = this.editorManager.currentEditor;
                const currentUri = currentEditor?.getResourceUri();
                if (this.model.canHandle(currentUri)) {
                    this.model.load(currentUri, currentEditor?.editor.document.getText()).catch(error => this.reportLoadFailure(error));
                }
            }, 250);
        });
        if (listener) {
            this.toDisposeOnEditorChange.push(listener);
        }
    }

    protected syncMarkers(): void {
        const summaryUri = this.model.summary?.uri;
        const currentUri = this.editorManager.currentEditor?.getResourceUri();
        const targetUri = this.model.canHandle(summaryUri) ? summaryUri : (this.model.canHandle(currentUri) ? currentUri : undefined);
        const failureMessage = this.model.error || (this.model.validation?.status === 'error' ? this.model.validation.message : undefined);

        if (this.lastMarkerUri && (!targetUri || this.lastMarkerUri.toString() !== targetUri.toString())) {
            this.problemManager.setMarkers(this.lastMarkerUri, this.markerOwner, []);
            this.lastMarkerUri = undefined;
        }
        if (!targetUri) {
            return;
        }
        if (!failureMessage) {
            this.problemManager.setMarkers(targetUri, this.markerOwner, []);
            this.lastMarkerUri = targetUri;
            return;
        }

        const lineMatch = /line\s+(\d+)\s*:/i.exec(failureMessage);
        const lineZeroBased = lineMatch ? Math.max(0, parseInt(lineMatch[1], 10) - 1) : 0;
        this.problemManager.setMarkers(targetUri, this.markerOwner, [{
            severity: DiagnosticSeverity.Error,
            message: failureMessage,
            source: 'sbagenxlib',
            range: {
                start: { line: lineZeroBased, character: 0 },
                end: { line: lineZeroBased, character: 1 }
            }
        }]);
        this.lastMarkerUri = targetUri;
    }
}
