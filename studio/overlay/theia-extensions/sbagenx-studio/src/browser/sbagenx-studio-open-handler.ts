/********************************************************************************
 * Copyright (C) 2026 SBaGenX contributors.
 *
 * This program and the accompanying materials are made available under the
 * terms of the MIT License, which is available in the project root.
 *
 * SPDX-License-Identifier: MIT
 ********************************************************************************/

import URI from '@theia/core/lib/common/uri';
import { OpenHandler } from '@theia/core/lib/browser';
import { inject, injectable } from '@theia/core/shared/inversify';
import { EditorManager } from '@theia/editor/lib/browser/editor-manager';

@injectable()
export class SbagenxStudioOpenHandler implements OpenHandler {

    readonly id = 'sbagenx-studio-text-editor-opener';
    readonly label = 'SBaGenX Text Editor';

    @inject(EditorManager)
    protected readonly editorManager: EditorManager;

    canHandle(uri: URI): number {
        const ext = uri.path.ext.toLowerCase();
        if (ext === '.sbg' || ext === '.sbgf') {
            return 200000;
        }
        return 0;
    }

    open(uri: URI): Promise<object | undefined> {
        return this.editorManager.open(uri);
    }
}
