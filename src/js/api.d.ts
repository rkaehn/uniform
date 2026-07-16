interface UiApi {
    nodeNull: number;
    pointerNull: number;
    keyAuto: undefined;
    key(value: UiKey): number | undefined;
    nodeMake(gen: number, idx: number): number;
    nodeGetGen(node: number): number;
    nodeGetIdx(node: number): number;
    nodeIsNull(node: number): boolean;
    nodeIsEqual(a: number, b: number): boolean;
    pointerMake(gen: number, idx: number): number;
    pointerGetGen(pointer: number): number;
    pointerGetIdx(pointer: number): number;
    pointerIsNull(pointer: number): boolean;
    pointerIsEqual(a: number, b: number): boolean;
    rgbaCs(r: number, g: number, b: number, a: number, colorSpace: UiColorSpace | number): UiColor;
    rgba(r: number, g: number, b: number, a: number): UiColor;
    hexA(hex: number, alpha: number): UiColor;
    hex(hex: number): UiColor;
    grayA(x: number, alpha: number): UiColor;
    gray(x: number): UiColor;
    white: UiColor;
    gray90: UiColor;
    gray80: UiColor;
    gray70: UiColor;
    gray60: UiColor;
    gray50: UiColor;
    gray40: UiColor;
    gray30: UiColor;
    gray20: UiColor;
    gray10: UiColor;
    black: UiColor;
    red: UiColor;
    green: UiColor;
    blue: UiColor;
    yellow: UiColor;
    magenta: UiColor;
    cyan: UiColor;
    clear: UiColor;
    pad(x: number): number[];
    padXy(x: number, y: number): number[];
    fontCreate(rawData: ArrayBufferView | ArrayBuffer): number;
    fontDestroy(font: number): void;
    customDrawSolid(info: UiCustomOnRender, transform: number[] | undefined, color: UiColorInit, vertices: UiCustomVertexSolid[] | Float32Array): void;
    customDrawGraphic(info: UiCustomOnRender, transform: number[] | undefined, graphic: number, tint: UiColorInit, vertices: UiCustomVertexGraphic[] | Float32Array): void;
}

interface LogApi {
    s(str: unknown): void;
}

declare const uniform: {
    args: string[];
    exit(code?: number): void;
    alloc(size: number): number;
    free(ptr: number): void;
    read(ptr: number, len: number): Uint8Array;
    write(ptr: number, bytes: ArrayBufferView | ArrayBuffer): void;
    sizeof(name: string): number;
    readStruct(name: string, ptr: number): any;
    writeStruct(name: string, ptr: number, value: any): void;
};
