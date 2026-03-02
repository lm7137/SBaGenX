/********************************************************************************
 * Copyright (C) 2026 SBaGenX contributors.
 *
 * This program and the accompanying materials are made available under the
 * terms of the MIT License, which is available in the project root.
 *
 * SPDX-License-Identifier: MIT
 ********************************************************************************/

import * as monaco from '@theia/monaco-editor-core';

const SBG_LANGUAGE_ID = 'sbagenx-sbg';
const SBGF_LANGUAGE_ID = 'sbagenx-sbgf';

const sbgTokenizer: monaco.languages.IMonarchLanguage = {
    defaultToken: '',
    tokenPostfix: '.sbg',
    ignoreCase: false,
    brackets: [
        { open: '{', close: '}', token: 'delimiter.curly' },
        { open: '[', close: ']', token: 'delimiter.square' },
        { open: '(', close: ')', token: 'delimiter.parenthesis' }
    ],
    tokenizer: {
        root: [
            [/^\s*(#|;|\/\/).*$/, 'comment'],
            [/(#|;|\/\/).*$/, 'comment'],
            [/\b(?:NOW(?:\+\d{1,2}:\d{2}(?::\d{2})?)?|\+\d{1,2}:\d{2}(?::\d{2})?|\d{1,2}:\d{2}(?::\d{2})?)\b/, 'number'],
            [/[A-Za-z_][\w-]*(?=\s*:\s*\{)/, 'type.identifier'],
            [/[A-Za-z_][\w-]*(?=\s*:)/, 'predefined'],
            [/\b-(?:A|B|C|D|E|F|G|H|I|L|M|P|Q|R|S|T|U|V|W|X|Y|Z|e|f|i|m|o|p|q|r|s|t|u|w)\b/, 'keyword'],
            [/\b(?:mixspin|mixpulse|mixbeat|mixam)(?=:)/, 'keyword'],
            [/\bmix(?=\/)/, 'keyword'],
            [/\b(?:drop|sigmoid|slide|curve)\b/, 'type'],
            [/\b(?:bell|pink|brown|white|spin:.*?|wave\d\d)\b/, 'type'],
            [/[+\-]?[0-9]+(?:\.[0-9]+)?(?:[eE][+\-]?[0-9]+)?/, 'number.float'],
            [/[{}()[\]]/, '@brackets'],
            [/[:,/@^~]|==|->|<=|>=|!=|[+\-*\/=<>]/, 'operator'],
            [/"([^"\\]|\\.)*$/, 'string.invalid'],
            [/"/, 'string', '@string'],
            [/[A-Za-z_][\w-]*/, 'identifier']
        ],
        string: [
            [/[^\\"]+/, 'string'],
            [/\\./, 'string.escape'],
            [/"/, 'string', '@pop']
        ]
    }
};

const sbgfTokenizer: monaco.languages.IMonarchLanguage = {
    defaultToken: '',
    tokenPostfix: '.sbgf',
    ignoreCase: false,
    brackets: [
        { open: '{', close: '}', token: 'delimiter.curly' },
        { open: '[', close: ']', token: 'delimiter.square' },
        { open: '(', close: ')', token: 'delimiter.parenthesis' }
    ],
    tokenizer: {
        root: [
            [/^\s*(#|;|\/\/).*$/, 'comment'],
            [/(#|;|\/\/).*$/, 'comment'],
            [/\b(?:param|solve)\b/, 'keyword'],
            [/\b(?:beat|carrier|amp|mixamp)(?=\s*(?:[<>]=?|=))/, 'keyword'],
            [/[A-Za-z_]\w*(?=\s*=)/, 'variable.name'],
            [/\b(?:m|s)(?=\b)/, 'keyword.unit'],
            [/\b(?:true|false)\b/, 'keyword'],
            [/\bpi\b|\be\b/, 'constant'],
            [/\b(?:ifelse|step|clamp|lerp|ramp|smoothstep|smootherstep|between|pulse|lt|le|gt|ge|eq|ne|seg|min2|max2|sin|cos|tan|asin|acos|atan|atan2|sinh|cosh|tanh|exp|log|log10|sqrt|abs|ceil|floor|round|pow|mod)(?=\s*\()/, 'predefined'],
            [/[+\-]?[0-9]+(?:\.[0-9]+)?(?:[eE][+\-]?[0-9]+)?/, 'number.float'],
            [/[{}()[\]]/, '@brackets'],
            [/[:,]|<=|>=|==|!=|[+\-*\/=<>]/, 'operator'],
            [/[A-Za-z_]\w*(?=\s*\()/, 'function'],
            [/[A-Za-z_]\w*/, 'identifier']
        ]
    }
};

function registerLanguage(
    id: string,
    aliases: string[],
    extensions: string[],
    tokenizer: monaco.languages.IMonarchLanguage
): void {
    if (monaco.languages.getLanguages().some(language => language.id === id)) {
        return;
    }
    monaco.languages.register({ id, aliases, extensions });
    monaco.languages.setLanguageConfiguration(id, {
        comments: {
            lineComment: '#'
        },
        brackets: [
            ['{', '}'],
            ['[', ']'],
            ['(', ')']
        ],
        autoClosingPairs: [
            { open: '{', close: '}' },
            { open: '[', close: ']' },
            { open: '(', close: ')' },
            { open: '"', close: '"' }
        ],
        surroundingPairs: [
            { open: '{', close: '}' },
            { open: '[', close: ']' },
            { open: '(', close: ')' },
            { open: '"', close: '"' }
        ]
    });
    monaco.languages.setMonarchTokensProvider(id, tokenizer);
}

registerLanguage(
    SBG_LANGUAGE_ID,
    ['SBaGen Sequence', 'sbg'],
    ['.sbg'],
    sbgTokenizer
);

registerLanguage(
    SBGF_LANGUAGE_ID,
    ['SBaGen Function', 'sbgf'],
    ['.sbgf'],
    sbgfTokenizer
);
