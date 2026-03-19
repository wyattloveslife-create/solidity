// SPDX-License-Identifier: GPL-3.0
// Adapted from https://github.com/Uniswap/merkle-distributor
pragma solidity >=0.8.0 <0.9.0;

/**
 * @title MerkleProof
 * @dev Merkle proof verification using sorted-pair hashing.
 */
library MerkleProof {
    /**
     * @dev Returns true if a `leaf` can be proved to be a part of a Merkle tree
     * defined by `root`. For this, a `proof` must be provided, containing
     * sibling hashes on the branch from the leaf to the root of the tree.
     * Each pair of leaves and each pair of pre-images are assumed to be sorted.
     */
    function verify(
        bytes32[] memory proof,
        bytes32 root,
        bytes32 leaf
    ) internal pure returns (bool) {
        bytes32 computedHash = leaf;

        for (uint256 i = 0; i < proof.length; i++) {
            bytes32 proofElement = proof[i];

            if (computedHash <= proofElement) {
                computedHash = keccak256(abi.encodePacked(computedHash, proofElement));
            } else {
                computedHash = keccak256(abi.encodePacked(proofElement, computedHash));
            }
        }

        return computedHash == root;
    }
}

/**
 * @title MerkleDistributor
 * @dev Distributes tokens to a predefined set of recipients using a Merkle tree.
 * Each recipient can claim tokens by providing a valid Merkle proof.
 *
 * Leaf hashes are computed as keccak256(abi.encode(account, param1, param2)).
 */
contract MerkleDistributor {
    bytes32 public immutable merkleRoot;

    /// @dev Tracks which leaves have already been claimed.
    mapping(bytes32 => bool) private _claimed;

    event Claimed(address indexed account, uint256 param1, uint256 param2);

    error AlreadyClaimed();
    error InvalidProof();

    constructor(bytes32 merkleRoot_) {
        merkleRoot = merkleRoot_;
    }

    /**
     * @notice Returns true if the leaf derived from the given parameters has
     * already been claimed.
     */
    function isClaimed(bytes32 leaf) public view returns (bool) {
        return _claimed[leaf];
    }

    /**
     * @notice Claim for `account` using the provided Merkle `proof`.
     * @param account  Recipient address.
     * @param param1   First encoded claim parameter.
     * @param param2   Second encoded claim parameter.
     * @param proof    Merkle proof path from leaf to root.
     */
    function claim(
        address account,
        uint256 param1,
        uint256 param2,
        bytes32[] calldata proof
    ) external {
        bytes32 leaf = keccak256(abi.encode(account, param1, param2));

        if (isClaimed(leaf)) revert AlreadyClaimed();

        if (!MerkleProof.verify(proof, merkleRoot, leaf)) revert InvalidProof();

        _claimed[leaf] = true;
        emit Claimed(account, param1, param2);
    }
}
