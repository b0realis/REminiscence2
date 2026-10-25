// SPDX-License-Identifier: GPL-3.0-or-later
#import <Cocoa/Cocoa.h>
#include <unistd.h>

// Empty edition folders, Finder metadata and instructions are not game data.
static BOOL hasAssets(NSString *path) {
    NSFileManager *manager = [NSFileManager defaultManager];
    NSDirectoryEnumerator *files = [manager enumeratorAtPath:path];
    for (NSString *file in files) {
        if ([[file lastPathComponent] hasPrefix:@"."]) { [files skipDescendants]; continue; }
        if ([[[file lastPathComponent] lowercaseString] hasPrefix:@"readme"]) continue;
        BOOL directory = NO;
        if ([manager fileExistsAtPath:[path stringByAppendingPathComponent:file] isDirectory:&directory] && !directory)
            return YES;
    }
    return NO;
}
int main(int argc, char **argv) {
    @autoreleasepool {
        NSBundle *main = [NSBundle mainBundle];
        NSString *bundle = [main bundlePath];
        NSString *resources = [main resourcePath];
        NSString *root = [bundle stringByDeletingLastPathComponent];
        // Public apps keep user data outside the signed bundle. Replacing an app
        // in Applications therefore preserves all editions, settings and saves.
        NSString *support = [NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES) firstObject];
        NSString *assets = [support stringByAppendingPathComponent:@"REminiscence2/assets"];
        for (NSString *candidate in @[[resources stringByAppendingPathComponent:@"assets"], [root stringByAppendingPathComponent:@"assets"], assets]) {
            if (hasAssets(candidate)) { assets = candidate; break; }
        }
        BOOL cli = NO;
        for (int i = 1; i < argc; ++i) {
            NSString *arg = [NSString stringWithUTF8String:argv[i]];
            if ([arg isEqualToString:@"-h"] || [arg isEqualToString:@"--help"] ||
                [arg isEqualToString:@"--version"] || [arg isEqualToString:@"--list-assets"] ||
                [arg isEqualToString:@"--datapath"] || [arg hasPrefix:@"--datapath="])
                cli = YES;
        }
        if (!cli && !hasAssets(assets)) {
            NSError *error = nil;
            if (![[NSFileManager defaultManager] createDirectoryAtPath:assets withIntermediateDirectories:YES attributes:nil error:&error]) {
                fprintf(stderr, "Cannot create assets folder: %s\n", [[error localizedDescription] UTF8String]);
                return 1;
            }
            [NSApplication sharedApplication];
            NSAlert *alert = [[NSAlert alloc] init];
            [alert setMessageText:@"REminiscence2 needs your game files"];
            [alert setInformativeText:[NSString stringWithFormat:@"Place your prepared Flashback editions in separate folders inside %@, then open the app again. The compact_assets.py tool can prepare them.", assets]];
            [alert addButtonWithTitle:@"Open assets folder"];
            [alert addButtonWithTitle:@"Quit"];
            if ([alert runModal] == NSAlertFirstButtonReturn)
                [[NSWorkspace sharedWorkspace] openURL:[NSURL fileURLWithPath:assets]];
            return 0;
        }
        NSString *engine = [bundle stringByAppendingPathComponent:@"Contents/MacOS/reminiscence2"];
        NSString *option = [@"--datapath=" stringByAppendingString:assets];
        if ([[NSFileManager defaultManager] fileExistsAtPath:[resources stringByAppendingPathComponent:@"fb.cfg"]])
            chdir([resources fileSystemRepresentation]);
        char **arguments = calloc((size_t)argc + 2, sizeof(char *));
        if (!arguments) return 1;
        arguments[0] = (char *)[engine fileSystemRepresentation];
        arguments[1] = (char *)[option UTF8String];
        for (int i = 1; i < argc; ++i) arguments[i + 1] = argv[i];
        execv(arguments[0], arguments); perror("REminiscence2"); return 1;
    }
}
