//
// Created by Tristan on 2/1/26.
//

#ifndef REFACTOREDCLONE_COLLISION_HPP
#define REFACTOREDCLONE_COLLISION_HPP

// In ChunkHelper or a new Collision.hpp

struct AABB {
    Vector3 min{};
    Vector3 max{};
};

class Collision {
public:
    // Check if a point is inside a solid block
    static bool isBlockSolid(int worldX, int worldY, int worldZ) {
        if (worldY < 0 || worldY >= CHUNK_SIZE_Y) return false;

        int blockId = ChunkHelper::getBlock(worldX, worldY, worldZ);

        // Air and water are not solid
        if (blockId == ID_AIR || blockId == ID_WATER) return false;

        return true;
    }

    // Check if an AABB collides with any solid blocks
    static bool checkCollision(const AABB &box) {
        int minX = (int) floor(box.min.x);
        int minY = (int) floor(box.min.y);
        int minZ = (int) floor(box.min.z);
        int maxX = (int) floor(box.max.x);
        int maxY = (int) floor(box.max.y);
        int maxZ = (int) floor(box.max.z);

        for (int x = minX; x <= maxX; x++) {
            for (int y = minY; y <= maxY; y++) {
                for (int z = minZ; z <= maxZ; z++) {
                    if (isBlockSolid(x, y, z)) {
                        return true;
                    }
                }
            }
        }

        return false;
    }

    // Get player AABB from position (position is at feet)
    static AABB getPlayerAABB(const Vector3 &position) {
        const float PLAYER_WIDTH = 0.6f;
        const float PLAYER_HEIGHT = 1.8f;
        const float HALF_WIDTH = PLAYER_WIDTH / 2.0f;

        return {
            {position.x - HALF_WIDTH, position.y, position.z - HALF_WIDTH},
            {position.x + HALF_WIDTH, position.y + PLAYER_HEIGHT, position.z + HALF_WIDTH}
        };
    }
};

#endif //REFACTOREDCLONE_COLLISION_HPP
