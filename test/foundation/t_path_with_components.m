/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#import <Foundation/Foundation.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Differential test verifying our Foundation +[NSString pathWithComponents:]
 * behavior matches macOS Foundation across empty inputs, nil exception,
 * leading/trailing/repeated slashes, Unicode, and pathComponents roundtrips.
 */
@interface NSString (KidoPathWithComponentsCheck)
+ (NSString *)kido_pathWithComponents:(NSArray<NSString *> *)components;
@end

@implementation NSString (KidoPathWithComponentsCheck)

+ (NSString *)kido_pathWithComponents:(NSArray<NSString *> *)components {
    if (components == nil) {
        [NSException raise:NSInvalidArgumentException
                    format:@"*** +[NSString pathWithComponents:]: nil argument"];
    }

    NSUInteger count = [components count];
    if (count == 0) {
        return @"";
    }

    NSMutableString *result = [NSMutableString string];
    BOOL isAbsolute = NO;
    BOOL sawNonEmpty = NO;
    BOOL hasSegment = NO;

    for (NSUInteger i = 0; i < count; i++) {
        NSString *comp = [components objectAtIndex:i];
        NSUInteger len = [comp length];
        if (len == 0) {
            continue;
        }
        if (!sawNonEmpty) {
            sawNonEmpty = YES;
            if ([comp characterAtIndex:0] == '/') {
                isAbsolute = YES;
            }
        }

        NSUInteger pos = 0;
        while (pos < len) {
            while (pos < len && [comp characterAtIndex:pos] == '/') {
                pos++;
            }
            if (pos >= len) {
                break;
            }
            NSUInteger segStart = pos;
            while (pos < len && [comp characterAtIndex:pos] != '/') {
                pos++;
            }
            if (hasSegment || isAbsolute) {
                [result appendString:@"/"];
            }
            [result appendString:[comp substringWithRange:NSMakeRange(segStart, pos - segStart)]];
            hasSegment = YES;
        }
    }

    if (!hasSegment) {
        return isAbsolute ? @"/" : @"";
    }
    return [NSString stringWithString:result];
}

@end

int main(void) {
    @autoreleasepool {
        int passed = 0;
        int failed = 0;

        NSString *appleNilExc = nil;
        NSString *kidoNilExc = nil;
        NSArray<NSString *> *nilArr = nil;
        @try {
            (void)[NSString pathWithComponents:nilArr];
        } @catch (NSException *e) {
            appleNilExc = [e name];
        }
        @try {
            (void)[NSString kido_pathWithComponents:nilArr];
        } @catch (NSException *e) {
            kidoNilExc = [e name];
        }
        if ((appleNilExc == nil && kidoNilExc == nil) ||
            (appleNilExc != nil && [appleNilExc isEqualToString:kidoNilExc])) {
            passed++;
        } else {
            fprintf(stderr, "FAIL nil: apple=%s kido=%s\n",
                    appleNilExc ? [appleNilExc UTF8String] : "(none)",
                    kidoNilExc ? [kidoNilExc UTF8String] : "(none)");
            failed++;
        }

        NSArray<NSArray<NSString *> *> *cases = @[
            @[],
            @[@""],
            @[@"", @""],
            @[@"/"],
            @[@"//"],
            @[@"///"],
            @[@"/", @"/"],
            @[@"/", @"//", @"///"],
            @[@"", @"/"],
            @[@"/", @""],
            @[@"", @"/", @""],
            @[@"", @"/a"],
            @[@"", @"/a", @"b"],
            @[@"", @"a", @"b"],
            @[@"a"],
            @[@"a", @"b"],
            @[@"a", @"b", @"c"],
            @[@"/", @"a"],
            @[@"/", @"a", @"b"],
            @[@"/", @"a", @"b", @"/"],
            @[@"/", @"a", @"/"],
            @[@"a", @"/"],
            @[@"a", @"b", @"/"],
            @[@"a/", @"b"],
            @[@"a/", @"/b"],
            @[@"a/", @"/b/"],
            @[@"a//", @"//b///", @"c//"],
            @[@"///a", @"b"],
            @[@"//a//b", @"//c//d//"],
            @[@"a", @"", @"b"],
            @[@"/", @"", @"a", @"", @"b", @""],
            @[@"a", @"", @"/"],
            @[@"/", @"usr", @"local", @"bin", @"tool"],
            @[@"~", @"Documents", @"file.txt"],
            @[@".", @"a", @"..", @"b"],
            @[@"..", @".."],
            @[@"/", @"..", @"a"],
            @[@"a/b/c"],
            @[@"/a/b/c/"],
            @[@"/a/b/c/", @"/"],
            @[@"a/b/c/", @"/"],
            @[@"/", @"foo bar", @"baz qux.dat"],
            @[@"/", @"日本語", @"パス", @"モデル.dat"],
            @[@"dir🚀", @"sub/file✨.bin"],
            @[@"", @"///", @""],
            @[@"a", @"///", @"b"],
            @[@"a", @"/", @"/", @"b", @"/"],
            @[@" ", @" "],
            @[@"/", @" "],
            @[@"a.b", @"c.d"],
            @[@"/a//b", @"", @"/c/", @"d/"],
            @[@"a", @"/b"],
            @[@"a", @"//b"],
            @[@"a", @"/b/"],
            @[@"/a", @"b", @"c", @""],
            @[@"/a/", @""],
            @[@"a/", @""],
        ];

        for (NSUInteger i = 0; i < [cases count]; i++) {
            NSArray<NSString *> *c = [cases objectAtIndex:i];
            NSString *expected = [NSString pathWithComponents:c];
            NSString *actual = [NSString kido_pathWithComponents:c];
            if ([expected isEqualToString:actual]) {
                passed++;
            } else {
                fprintf(stderr, "FAIL case %lu: expected=[%s] actual=[%s]\n",
                        (unsigned long)i, [expected UTF8String], [actual UTF8String]);
                failed++;
            }
        }

        NSArray<NSString *> *roundtrips = @[
            @"/",
            @"/usr/local/bin",
            @"a/b/c",
            @"single",
            @"",
            @"/a/b/c/",
            @"a/b/",
        ];
        for (NSUInteger i = 0; i < [roundtrips count]; i++) {
            NSString *p = [roundtrips objectAtIndex:i];
            NSArray<NSString *> *comps = [p pathComponents];
            NSString *expected = [NSString pathWithComponents:comps];
            NSString *actual = [NSString kido_pathWithComponents:comps];
            if ([expected isEqualToString:actual]) {
                passed++;
            } else {
                fprintf(stderr, "FAIL roundtrip [%s]: expected=[%s] actual=[%s]\n",
                        [p UTF8String], [expected UTF8String], [actual UTF8String]);
                failed++;
            }
        }

        printf("t_path_with_components: %d passed, %d failed\n", passed, failed);
        return failed == 0 ? 0 : 1;
    }
}
