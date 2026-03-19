# Merkle Distributor

A Solidity implementation of a Merkle proof-based token distribution contract.

Imported from https://github.com/Uniswap/merkle-distributor

## Overview

The `MerkleDistributor` contract enables token distribution to a predefined set of
recipients using a Merkle tree. Each recipient can trustlessly claim their allocation
by submitting a Merkle proof.

Leaf hashes are computed as `keccak256(abi.encode(account, param1, param2))`.
The tree is built with sorted-pair hashing: at each level, sibling nodes are sorted
before hashing so that `hash(a, b) = keccak256(a < b ? a ++ b : b ++ a)`.
Odd-length layers are handled by duplicating the last node.

## Contract

```solidity
interface IMerkleDistributor {
    /// @notice Returns the Merkle root of the distribution tree.
    function merkleRoot() external view returns (bytes32);

    /// @notice Returns true if the given leaf has already been claimed.
    function isClaimed(bytes32 leaf) external view returns (bool);

    /// @notice Claim tokens for the given account.
    /// @param account  Recipient address.
    /// @param param1   First encoded parameter (e.g. amount or index).
    /// @param param2   Second encoded parameter (e.g. index or amount).
    /// @param proof    Merkle proof path from leaf to root.
    function claim(
        address account,
        uint256 param1,
        uint256 param2,
        bytes32[] calldata proof
    ) external;
}
```

## Leaf Construction

Each leaf is the `keccak256` hash of the ABI-encoded claim parameters:

```javascript
// leaf = keccak256(abi.encode(account, param1, param2))
const leaf = ethers.utils.keccak256(
    ethers.utils.defaultAbiCoder.encode(
        ["address", "uint256", "uint256"],
        [account, param1, param2]
    )
);
```

## Proof Verification

On-chain verification using OpenZeppelin's `MerkleProof` library:

```solidity
bytes32 leaf = keccak256(abi.encode(account, param1, param2));
require(MerkleProof.verify(proof, merkleRoot, leaf), "Invalid proof");
```

## Test Data

The following test vectors use a 3-leaf tree with one duplicated node.

### Merkle Tree (JavaScript Format)

The object below represents the complete tree structure as produced by a
standard sorted-pair Merkle tree implementation.

```javascript
const merkleData = {
  leaves: [

    0x69d3ca75db69c48c0569d359a5f110f5101ae898fe7a89e9537aa4a487110801,

    0x6fcec51a48c67ee2de86adc83fb1d9e65b8b8c8f60548cd839e3c463c9e5a46a,

    0xd38fcdc03d82a6257cc8de426b5cb3e43a49073781a20be0c80ed1a3372a139a,

  ],
  layers: [
    [

      0x69d3ca75db69c48c0569d359a5f110f5101ae898fe7a89e9537aa4a487110801,

      0x6fcec51a48c67ee2de86adc83fb1d9e65b8b8c8f60548cd839e3c463c9e5a46a,

      0xd38fcdc03d82a6257cc8de426b5cb3e43a49073781a20be0c80ed1a3372a139a,

    ],
    [

      0x8681a8817cfd31888b4c697a38991e6f3be06accbdb36459e8bf5e92c2eb6600,

      0x5074756108d06d9e89bfa45aa7fcf1ab486e98cffce378a9bc71098e5687cb84,

    ],
    [

      0x97c7f98805481c199f21f29a2390071af3f73b91e19797d5a5d6f6c8bed296c6,

    ],
  ],
  proofs: {

    0x6fcec51a48c67ee2de86adc83fb1d9e65b8b8c8f60548cd839e3c463c9e5a46a: [

      0x69d3ca75db69c48c0569d359a5f110f5101ae898fe7a89e9537aa4a487110801,

      0x5074756108d06d9e89bfa45aa7fcf1ab486e98cffce378a9bc71098e5687cb84,

    ],

    0xd38fcdc03d82a6257cc8de426b5cb3e43a49073781a20be0c80ed1a3372a139a: [

      0xd38fcdc03d82a6257cc8de426b5cb3e43a49073781a20be0c80ed1a3372a139a,

      0x8681a8817cfd31888b4c697a38991e6f3be06accbdb36459e8bf5e92c2eb6600,

    ],
  },
  root: 0x97c7f98805481c199f21f29a2390071af3f73b91e19797d5a5d6f6c8bed296c6,
}
```

### Test Cases (JSON Format)

Each entry provides the raw inputs, the Merkle proof path, the expected root, and
the resulting leaf hash. Pass these directly to `claim()` for end-to-end testing.

```json
[
  {
    "inputs": [

      "0x599a9d94b12dd3313211bd1ae9e35a30c0753f5e",

      "0",

      "125000000000000000000"

    ],
    "proof": [

      "0x6fcec51a48c67ee2de86adc83fb1d9e65b8b8c8f60548cd839e3c463c9e5a46a",

      "0x5074756108d06d9e89bfa45aa7fcf1ab486e98cffce378a9bc71098e5687cb84"

    ],
    "root":
      "0x97c7f98805481c199f21f29a2390071af3f73b91e19797d5a5d6f6c8bed296c6",
    "leaf":
      "0x69d3ca75db69c48c0569d359a5f110f5101ae898fe7a89e9537aa4a487110801"
  },
  {
    "inputs": [

      "0x599a9d94b12dd3313211bd1ae9e35a30c0753f5e",

      "250000000000000000000",

      "0"

    ],
    "proof": [

      "0x69d3ca75db69c48c0569d359a5f110f5101ae898fe7a89e9537aa4a487110801",

      "0x5074756108d06d9e89bfa45aa7fcf1ab486e98cffce378a9bc71098e5687cb84"

    ],
    "root":
      "0x97c7f98805481c199f21f29a2390071af3f73b91e19797d5a5d6f6c8bed296c6",
    "leaf":
      "0x6fcec51a48c67ee2de86adc83fb1d9e65b8b8c8f60548cd839e3c463c9e5a46a"
  },
  {
    "inputs": [

      "0x599a9d94b12dd3313211bd1ae9e35a30c0753f5e",

      "0",

      "0"

    ],
    "proof": [

      "0xd38fcdc03d82a6257cc8de426b5cb3e43a49073781a20be0c80ed1a3372a139a",

      "0x8681a8817cfd31888b4c697a38991e6f3be06accbdb36459e8bf5e92c2eb6600"

    ],
    "root":
      "0x97c7f98805481c199f21f29a2390071af3f73b91e19797d5a5d6f6c8bed296c6",
    "leaf":
      "0xd38fcdc03d82a6257cc8de426b5cb3e43a49073781a20be0c80ed1a3372a139a"
  }
]
```
