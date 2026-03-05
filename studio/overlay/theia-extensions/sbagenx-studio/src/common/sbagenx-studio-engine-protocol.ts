/********************************************************************************
 * Copyright (C) 2026 SBaGenX contributors.
 *
 * This program and the accompanying materials are made available under the
 * terms of the MIT License, which is available in the project root.
 *
 * SPDX-License-Identifier: MIT
 ********************************************************************************/

export const SBAGENX_STUDIO_ENGINE_PATH = '/services/sbagenx-studio/engine';
export const SbagenxStudioEngineService = Symbol('SbagenxStudioEngineService');

export type SbagenxStudioValidationStatus = 'ok' | 'error' | 'unsupported';
export type SbagenxStudioBackendKind = 'sbagenxlib';

export interface SbagenxStudioInspectionResult {
    status: SbagenxStudioValidationStatus;
    backend: SbagenxStudioBackendKind;
    fileType: 'sbg' | 'sbgf' | 'unknown';
    message?: string;
    version?: string;
    apiVersion?: number;
    sourceMode?: 'none' | 'static' | 'keyframes' | 'curve' | 'unknown';
    voiceCount?: number;
    keyframeCount?: number;
    durationSec?: number;
    looping?: boolean;
    hasMixAmpControl?: boolean;
    hasMixEffects?: boolean;
    parameterCount?: number;
    hasSolve?: boolean;
    hasCarrierExpr?: boolean;
    hasAmpExpr?: boolean;
    hasMixAmpExpr?: boolean;
    beatPieceCount?: number;
    carrierPieceCount?: number;
    ampPieceCount?: number;
    mixAmpPieceCount?: number;
}

export interface SbagenxStudioEngineService {
    inspectFile(filePath: string): Promise<SbagenxStudioInspectionResult>;
}
