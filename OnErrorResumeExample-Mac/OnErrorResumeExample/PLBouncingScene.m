//
//  PLBouncingScene.m
//  OnErrorResumeExample
//
//  Created by Landon Fuller on 3/28/14.
//  Copyright (c) 2014 Plausible Labs. All rights reserved.
//

#import "PLBouncingScene.h"

@implementation PLBouncingScene

- (id)initWithSize:(CGSize)size {
    if ((self = [super initWithSize:size]) == nil)
        return nil;
    
    SKSpriteNode *floor = [SKSpriteNode spriteNodeWithColor: [SKColor redColor] size: CGSizeMake(self.frame.size.width, 20)];
    floor.anchorPoint = CGPointMake(0.0f, 0.0f);
    floor.physicsBody = [SKPhysicsBody bodyWithEdgeLoopFromRect: floor.frame];
    floor.physicsBody.dynamic = NO;
    [self addChild: floor];
    
    /* Add a new ball every 0.5 seconds, forever. This also keeps our scene alive forever, which doesn't
     * matter for our demo app. */
    [NSTimer scheduledTimerWithTimeInterval: 0.25 target: self selector: @selector(addBall) userInfo: nil repeats:YES];

    return self;
}

- (void) addBall {
    uint32_t pos = arc4random_uniform(self.size.width);
    
    SKShapeNode *ball = [SKShapeNode node];
    ball.name = @"bouncer";
    ball.position = CGPointMake(pos, self.size.height);
    
    /* Create the drawing path for the ball */
    {
        CGPathRef path = CGPathCreateWithEllipseInRect(CGRectMake(-20, -20, 40, 40), NULL);
        ball.path = path;
        CGPathRelease(path);
    }
    
    /* Create a physics body that matches the drawing path */
    ball.physicsBody = [SKPhysicsBody bodyWithCircleOfRadius: 20.0];
    ball.physicsBody.dynamic = YES;
    ball.physicsBody.restitution = 0.8;

    [self addChild: ball];
}

- (void) didSimulatePhysics {
    [self enumerateChildNodesWithName: @"bouncer" usingBlock: ^(SKNode *node, BOOL *stop) {
        if (node.position.y < 0 || node.position.y + 40 > self.size.width) {
            [node removeFromParent];
        }
    }];
}

@end
