#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface Live2DObjCAdapter : NSObject

/// Init with a path to a .model3.json file
- (instancetype)initWithModelPath:(NSString *)path;

/// Draw the model (called from GLKView delegate)
- (void)drawInRect:(CGRect)rect;

/// Set drag position for gaze following (x, y in range -1.0 ~ 1.0)
- (void)setDragX:(float)x y:(float)y;

/// Play a motion by group name and index. Also plays associated audio.
- (void)playMotionGroup:(NSString *)group index:(NSInteger)index;

/// Get the number of motions in a group
- (NSInteger)motionCountForGroup:(NSString *)group;

/// Get the names of all available motion groups
- (NSArray<NSString *> *)availableMotionGroups;

/// Set model scale (default 1.0)
- (void)setScale:(float)scale;

/// Set expression by name: "normal", "happy", "angry", "sad", "surprised"
- (void)setExpression:(NSString *)name;

/// Release the model
- (void)releaseModel;

@end

NS_ASSUME_NONNULL_END
