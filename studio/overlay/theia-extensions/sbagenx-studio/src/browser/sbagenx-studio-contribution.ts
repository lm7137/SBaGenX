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
import { inject, injectable } from '@theia/core/shared/inversify';
import { FileDialogService } from '@theia/filesystem/lib/browser/file-dialog/file-dialog-service';
import { EditorManager } from '@theia/editor/lib/browser/editor-manager';
import { MessageService } from '@theia/core/lib/common/message-service';
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

    @inject(FileDialogService)
    protected readonly fileDialogService: FileDialogService;

    @inject(EditorManager)
    protected readonly editorManager: EditorManager;

    @inject(MessageService)
    protected readonly messageService: MessageService;

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
            const uri = editor?.getResourceUri();
            if (this.model.canHandle(uri)) {
                this.model.load(uri).catch(error => this.reportLoadFailure(error));
            }
        });

        const currentUri = this.editorManager.currentEditor?.getResourceUri();
        if (this.model.canHandle(currentUri)) {
            this.model.load(currentUri).catch(error => this.reportLoadFailure(error));
        }
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
        await this.editorManager.open(uri);
        await this.model.load(uri);
        await this.openView({ activate: true, reveal: true });
    }

    protected reportLoadFailure(error: unknown): void {
        const message = error instanceof Error ? error.message : String(error);
        this.messageService.error(`Failed to inspect SBaGenX session: ${message}`);
    }
}
