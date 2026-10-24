// SPDX-License-Identifier: GPL-3.0-or-later
#import <Cocoa/Cocoa.h>
#include <unistd.h>
int main(int argc, char **argv) {
    @autoreleasepool {
        NSString *bundle = [[NSBundle mainBundle] bundlePath];
        NSString *root = [bundle stringByDeletingLastPathComponent];
        NSString *assets = [root stringByAppendingPathComponent:@"assets"];
        NSDirectoryEnumerator *files = [[NSFileManager defaultManager] enumeratorAtPath:assets];
        BOOL ready = NO;
        for (NSString *file in files) if (![[file lastPathComponent] isEqualToString:@"README.txt"]) { ready = YES; break; }
        if (!ready) {
            [NSApplication sharedApplication];
            NSAlert *alert = [[NSAlert alloc] init];
            [alert setMessageText:@"REminiscence2 needs your game files"];
            [alert setInformativeText:@"Copy your original, prepared Flashback game files into the assets folder beside REminiscence2.app, then open the app again."];
            [alert runModal]; return 0;
        }
        NSString *engine = [bundle stringByAppendingPathComponent:@"Contents/MacOS/reminiscence2"];
        NSString *option = [@"--datapath=" stringByAppendingString:assets];
        char **arguments = calloc((size_t)argc + 2, sizeof(char *));
        if (!arguments) return 1;
        arguments[0] = (char *)[engine fileSystemRepresentation];
        arguments[1] = (char *)[option UTF8String];
        for (int i = 1; i < argc; ++i) arguments[i + 1] = argv[i];
        execv(arguments[0], arguments); perror("REminiscence2"); return 1;
    }
}
