// Copyright © 2017-2023 Trust Wallet.
//
// This file is part of Trust. The full Trust copyright notice, including
// terms governing use, modification, and redistribution, is contained in the
// file LICENSE at the root of the source code distribution tree.

#include "Base58.h"
#include "Bitcoin/Address.h"
#include "Bitcoin/OutPoint.h"
#include "Bitcoin/Script.h"
#include "Bitcoin/SegwitAddress.h"
#include "Bitcoin/SigHashType.h"
#include "Bitcoin/Transaction.h"
#include "Bitcoin/TransactionBuilder.h"
#include "Bitcoin/TransactionSigner.h"
#include "Hash.h"
#include "HexCoding.h"
#include "PrivateKey.h"
#include "TxComparisonHelper.h"
#include "proto/Bitcoin.pb.h"
#include "TestUtilities.h"

#include <TrustWalletCore/TWAnySigner.h>
#include <TrustWalletCore/TWBitcoinScript.h>
#include <TrustWalletCore/TWCoinType.h>
#include <TrustWalletCore/TWPublicKeyType.h>

#include <fstream>
#include <vector>
#include <iterator>
#include <cassert>
#include <gtest/gtest.h>

namespace TW::Bitcoin {

// clang-format off
SigningInput buildInputP2PKH(bool omitKey = false) {
    auto hash0 = parse_hex("fff7f7881a8099afa6940d42d1e7f6362bec38171ea3edf433541db4e4ad969f");
    auto hash1 = parse_hex("ef51e1b804cc89d182d279655c3aa89e815b1b309fe287d9b2b55d57b90ec68a");

    // Setup input
    SigningInput input;
    input.hashType = hashTypeForCoin(TWCoinTypeBitcoin);
    input.amount = 335'790'000;
    input.totalAmount = 335'790'000;
    input.byteFee = 1;
    input.toAddress = "1Bp9U1ogV3A14FMvKbRJms7ctyso4Z4Tcx";
    input.changeAddress = "1FQc5LdgGHMHEN9nwkjmz6tWkxhPpxBvBU";
    input.coinType = TWCoinTypeBitcoin;

    auto utxoKey0 = PrivateKey(parse_hex("bbc27228ddcb9209d7fd6f36b02f7dfa6252af40bb2f1cbc7a557da8027ff866"));
    auto pubKey0 = utxoKey0.getPublicKey(TWPublicKeyTypeSECP256k1);
    auto utxoPubkeyHash0 = Hash::ripemd(Hash::sha256(pubKey0.bytes));
    assert(hex(utxoPubkeyHash0) == "b7cd046b6d522a3d61dbcb5235c0e9cc97265457");
    if (!omitKey) {
        input.privateKeys.push_back(utxoKey0);
    }

    auto utxoKey1 = PrivateKey(parse_hex("619c335025c7f4012e556c2a58b2506e30b8511b53ade95ea316fd8c3286feb9"));
    auto pubKey1 = utxoKey1.getPublicKey(TWPublicKeyTypeSECP256k1);
    auto utxoPubkeyHash1 = Hash::ripemd(Hash::sha256(pubKey1.bytes));
    assert(hex(utxoPubkeyHash1) == "1d0f172a0ecb48aee1be1f2687d2963ae33f71a1");
    if (!omitKey) {
        input.privateKeys.push_back(utxoKey1);
    }

    auto utxo0Script = Script::buildPayToPublicKeyHash(utxoPubkeyHash0);
    Data scriptHash;
    utxo0Script.matchPayToPublicKeyHash(scriptHash);
    assert(hex(scriptHash) == "b7cd046b6d522a3d61dbcb5235c0e9cc97265457");

    UTXO utxo0;
    utxo0.script = utxo0Script;
    utxo0.amount = 625'000'000;
    utxo0.outPoint = OutPoint(hash0, 0, UINT32_MAX);
    input.utxos.push_back(utxo0);

    UTXO utxo1;
    utxo1.script = Script(parse_hex("0014"
                                    "1d0f172a0ecb48aee1be1f2687d2963ae33f71a1"));
    utxo1.amount = 600'000'000;
    utxo1.outPoint = OutPoint(hash1, 1, UINT32_MAX);
    input.utxos.push_back(utxo1);

    return input;
}

/// This test only checks if the transaction output will have an expected value.
/// It doesn't check correctness of the encoded representation.
/// Issue: https://github.com/trustwallet/wallet-core/issues/3273
TEST(BitcoinSigning, SignMaxAmount) {
    const auto privateKey = parse_hex("4646464646464646464646464646464646464646464646464646464646464646");
    const auto ownAddress = "bc1qhkfq3zahaqkkzx5mjnamwjsfpq2jk7z00ppggv";

    const auto revUtxoHash0 =
        parse_hex("07c42b969286be06fae38528c85f0a1ce508d4df837eb5ac4cf5f2a7a9d65fa8");
    const auto inPubKey0 =
        parse_hex("024bc2a31265153f07e70e0bab08724e6b85e217f8cd628ceb62974247bb493382");
    const auto inPubKeyHash0 = parse_hex("bd92088bb7e82d611a9b94fbb74a0908152b784f");
    const auto utxoScript0 = parse_hex("0014bd92088bb7e82d611a9b94fbb74a0908152b784f");

    const auto initialAmount = 10'189'533;
    const auto availableAmount = 10'189'534;
    const auto fee = 110;
    const auto amountWithoutFee = availableAmount - fee;
    // There shouldn't be any change
    const auto change = 0;

    Proto::SigningInput signingInput;
    signingInput.set_coin_type(TWCoinTypeBitcoin);
    signingInput.set_hash_type(TWBitcoinSigHashTypeAll);
    signingInput.set_amount(initialAmount);
    signingInput.set_byte_fee(1);
    signingInput.set_to_address("bc1q2dsdlq3343vk29runkgv4yc292hmq53jedfjmp");
    signingInput.set_change_address(ownAddress);
    signingInput.set_use_max_amount(true);

    *signingInput.add_private_key() = std::string(privateKey.begin(), privateKey.end());

    // Add UTXO
    auto utxo = signingInput.add_utxo();
    utxo->set_script(utxoScript0.data(), utxoScript0.size());
    utxo->set_amount(availableAmount);
    utxo->mutable_out_point()->set_hash(
        std::string(revUtxoHash0.begin(), revUtxoHash0.end()));
    utxo->mutable_out_point()->set_index(0);
    utxo->mutable_out_point()->set_sequence(UINT32_MAX);

    // Plan
    Proto::TransactionPlan plan;
    ANY_PLAN(signingInput, plan, TWCoinTypeBitcoin);
    // Plan is checked, assume it is accepted
    EXPECT_EQ(plan.amount(), amountWithoutFee);
    EXPECT_EQ(plan.available_amount(), availableAmount);
    EXPECT_EQ(plan.fee(), fee);
    EXPECT_EQ(plan.change(), change);

    *signingInput.mutable_plan() = plan;

    Proto::SigningOutput output;
    ANY_SIGN(signingInput, TWCoinTypeBitcoin);

    const auto& output0 = output.transaction().outputs().at(0);
    EXPECT_EQ(output0.value(), amountWithoutFee);
}

TEST(BitcoinSigning, SignBRC20TransferCommit) {
    auto privateKey = parse_hex("e253373989199da27c48680e3a3fc0f648d50f9a727ef17a7fe6a4dc3b159129");
    auto fullAmount = 26400;
    auto minerFee = 3000;
    auto brcInscribeAmount = 7000;
    auto forFeeAmount = fullAmount - brcInscribeAmount - minerFee;
    auto txId = parse_hex("089098890d2653567b9e8df2d1fbe5c3c8bf1910ca7184e301db0ad3b495c88e");

    PrivateKey key(privateKey);
    auto pubKey = key.getPublicKey(TWPublicKeyTypeSECP256k1);
    auto utxoPubKeyHash = Hash::ripemd(Hash::sha256(pubKey.bytes));
    auto inputP2wpkh = TW::Bitcoin::Script::buildPayToWitnessPublicKeyHash(utxoPubKeyHash);
    auto outputInscribe = TW::Bitcoin::Script::buildBRC20InscribeTransfer("oadf", 20, pubKey.bytes);

    Proto::SigningInput input;
    input.set_is_it_brc_operation(true);
    input.add_private_key(key.bytes.data(), key.bytes.size());
    input.set_coin_type(TWCoinTypeBitcoin);

    auto& utxo = *input.add_utxo();
    utxo.set_amount(fullAmount);
    utxo.set_script(inputP2wpkh.bytes.data(), inputP2wpkh.bytes.size());
    utxo.set_variant(Proto::TransactionVariant::P2WPKH);

    Proto::OutPoint out;
    out.set_index(1);
    out.set_hash(txId.data(), txId.size());
    *utxo.mutable_out_point() = out;

    Proto::TransactionPlan plan;
    auto& utxo1 = *plan.add_utxos();
    utxo1.set_amount(brcInscribeAmount);
    utxo1.set_script(outputInscribe.script());
    utxo1.set_variant(Proto::TransactionVariant::BRC20TRANSFER);

    auto& utxo2 = *plan.add_utxos();
    utxo2.set_amount(forFeeAmount);
    utxo2.set_script(inputP2wpkh.bytes.data(), inputP2wpkh.bytes.size());
    utxo2.set_variant(Proto::TransactionVariant::P2WPKH);

    *input.mutable_plan() = plan;
    Proto::SigningOutput output;

    ANY_SIGN(input, TWCoinTypeBitcoin);
    ASSERT_EQ(hex(output.encoded()), "02000000000101089098890d2653567b9e8df2d1fbe5c3c8bf1910ca7184e301db0ad3b495c88e0100000000ffffffff02581b000000000000225120e8b706a97732e705e22ae7710703e7f589ed13c636324461afa443016134cc051040000000000000160014e311b8d6ddff856ce8e9a4e03bc6d4fe5050a83d02483045022100a44aa28446a9a886b378a4a65e32ad9a3108870bd725dc6105160bed4f317097022069e9de36422e4ce2e42b39884aa5f626f8f94194d1013007d5a1ea9220a06dce0121030f209b6ada5edb42c77fd2bc64ad650ae38314c8f451f3e36d80bc8e26f132cb00000000");
    ASSERT_EQ(output.transaction_id(), "797d17d47ae66e598341f9dfdea020b04d4017dcf9cc33f0e51f7a6082171fb1");
    ASSERT_EQ(output.error(), Common::Proto::OK);

    // Successfully broadcasted: https://www.blockchain.com/explorer/transactions/btc/797d17d47ae66e598341f9dfdea020b04d4017dcf9cc33f0e51f7a6082171fb1
}

TEST(BitcoinSigning, SignBRC20TransferReveal) {
    auto privateKey = parse_hex("e253373989199da27c48680e3a3fc0f648d50f9a727ef17a7fe6a4dc3b159129");
    auto dustSatoshi = 546;
    auto brcInscribeAmount = 7000;
    auto txId = parse_hex("b11f1782607a1fe5f033ccf9dc17404db020a0dedff94183596ee67ad4177d79");

    PrivateKey key(privateKey);
    auto pubKey = key.getPublicKey(TWPublicKeyTypeSECP256k1);
    auto utxoPubKeyHash = Hash::ripemd(Hash::sha256(pubKey.bytes));
    auto inputP2wpkh = TW::Bitcoin::Script::buildPayToWitnessPublicKeyHash(utxoPubKeyHash);
    auto outputInscribe = TW::Bitcoin::Script::buildBRC20InscribeTransfer("oadf", 20, pubKey.bytes);

    Proto::SigningInput input;
    input.set_is_it_brc_operation(true);
    input.add_private_key(key.bytes.data(), key.bytes.size());
    input.set_coin_type(TWCoinTypeBitcoin);

    auto& utxo = *input.add_utxo();
    utxo.set_amount(brcInscribeAmount);
    utxo.set_script(outputInscribe.script());
    utxo.set_variant(Proto::TransactionVariant::BRC20TRANSFER);
    utxo.set_spendingscript(outputInscribe.spendingscript());

    Proto::OutPoint out;
    out.set_index(0);
    out.set_hash(txId.data(), txId.size());
    *utxo.mutable_out_point() = out;

    Proto::TransactionPlan plan;
    auto& utxo1 = *plan.add_utxos();
    utxo1.set_amount(dustSatoshi);
    utxo1.set_script(inputP2wpkh.bytes.data(), inputP2wpkh.bytes.size());
    utxo1.set_variant(Proto::TransactionVariant::P2WPKH);

    *input.mutable_plan() = plan;
    Proto::SigningOutput output;

    ANY_SIGN(input, TWCoinTypeBitcoin);
    auto result = hex(output.encoded());
    ASSERT_EQ(result.substr(0, 164), "02000000000101b11f1782607a1fe5f033ccf9dc17404db020a0dedff94183596ee67ad4177d790000000000ffffffff012202000000000000160014e311b8d6ddff856ce8e9a4e03bc6d4fe5050a83d0340");
    ASSERT_EQ(result.substr(292, result.size() - 292), "5b0063036f7264010118746578742f706c61696e3b636861727365743d7574662d3800377b2270223a226272632d3230222c226f70223a227472616e73666572222c227469636b223a226f616466222c22616d74223a223230227d6821c00f209b6ada5edb42c77fd2bc64ad650ae38314c8f451f3e36d80bc8e26f132cb00000000");
    ASSERT_EQ(output.transaction_id(), "7046dc2689a27e143ea2ad1039710885147e9485ab6453fa7e87464aa7dd3eca");
    ASSERT_EQ(output.error(), Common::Proto::OK);

    // Successfully broadcasted: https://www.blockchain.com/explorer/transactions/btc/7046dc2689a27e143ea2ad1039710885147e9485ab6453fa7e87464aa7dd3eca
}

TEST(BitcoinSigning, SignBRC20TransferInscription) {
    auto privateKey = parse_hex("e253373989199da27c48680e3a3fc0f648d50f9a727ef17a7fe6a4dc3b159129");
    auto dustSatoshi = 546;
    auto brcInscribeAmount = 7000;
    auto fullAmount = 26400;
    auto minerFee = 3000;
    auto forFeeAmount = fullAmount - brcInscribeAmount - minerFee;
    auto txIDInscription = parse_hex("7046dc2689a27e143ea2ad1039710885147e9485ab6453fa7e87464aa7dd3eca");
    std::reverse(begin(txIDInscription), end(txIDInscription));
    auto txIDForFees = parse_hex("797d17d47ae66e598341f9dfdea020b04d4017dcf9cc33f0e51f7a6082171fb1");
    std::reverse(begin(txIDForFees), end(txIDForFees));

    PrivateKey key(privateKey);
    auto pubKey = key.getPublicKey(TWPublicKeyTypeSECP256k1);
    auto utxoPubKeyHash = Hash::ripemd(Hash::sha256(pubKey.bytes));
    auto utxoPubKeyHashBob = Hash::ripemd(Hash::sha256(parse_hex("02f453bb46e7afc8796a9629e89e07b5cb0867e9ca340b571e7bcc63fc20c43f2e")));
    auto inputP2wpkh = TW::Bitcoin::Script::buildPayToWitnessPublicKeyHash(utxoPubKeyHash);
    auto outputP2wpkh = TW::Bitcoin::Script::buildPayToWitnessPublicKeyHash(utxoPubKeyHashBob);
    auto outputInscribe = TW::Bitcoin::Script::buildBRC20InscribeTransfer("oadf", 20, pubKey.bytes);

    Proto::SigningInput input;
    input.set_is_it_brc_operation(true);
    input.add_private_key(key.bytes.data(), key.bytes.size());
    input.set_coin_type(TWCoinTypeBitcoin);

    auto& utxo0 = *input.add_utxo();
    utxo0.set_amount(dustSatoshi);
    utxo0.set_script(inputP2wpkh.bytes.data(), inputP2wpkh.bytes.size());
    utxo0.set_variant(Proto::TransactionVariant::P2WPKH);

    Proto::OutPoint out0;
    out0.set_index(0);
    out0.set_hash(txIDInscription.data(), txIDInscription.size());
    *utxo0.mutable_out_point() = out0;

    auto& utxo1 = *input.add_utxo();
    utxo1.set_amount(forFeeAmount);
    utxo1.set_script(inputP2wpkh.bytes.data(), inputP2wpkh.bytes.size());
    utxo1.set_variant(Proto::TransactionVariant::P2WPKH);

    Proto::OutPoint out1;
    out1.set_index(1);
    out1.set_hash(txIDForFees.data(), txIDForFees.size());
    *utxo1.mutable_out_point() = out1;

    Proto::TransactionPlan plan;
    auto& utxo2 = *plan.add_utxos();
    utxo2.set_amount(dustSatoshi);
    utxo2.set_script(outputP2wpkh.bytes.data(), outputP2wpkh.bytes.size());
    utxo2.set_variant(Proto::TransactionVariant::P2WPKH);

    auto& utxo3 = *plan.add_utxos();
    utxo3.set_amount(forFeeAmount - minerFee);
    utxo3.set_script(inputP2wpkh.bytes.data(), inputP2wpkh.bytes.size());
    utxo3.set_variant(Proto::TransactionVariant::P2WPKH);

    *input.mutable_plan() = plan;
    Proto::SigningOutput output;

    ANY_SIGN(input, TWCoinTypeBitcoin);
    auto result = hex(output.encoded());
    ASSERT_EQ(hex(output.encoded()), "02000000000102ca3edda74a46877efa5364ab85947e148508713910ada23e147ea28926dc46700000000000ffffffffb11f1782607a1fe5f033ccf9dc17404db020a0dedff94183596ee67ad4177d790100000000ffffffff022202000000000000160014e891850afc55b64aa8247b2076f8894ebdf889015834000000000000160014e311b8d6ddff856ce8e9a4e03bc6d4fe5050a83d024830450221008798393eb0b7390217591a8c33abe18dd2f7ea7009766e0d833edeaec63f2ec302200cf876ff52e68dbaf108a3f6da250713a9b04949a8f1dcd1fb867b24052236950121030f209b6ada5edb42c77fd2bc64ad650ae38314c8f451f3e36d80bc8e26f132cb0248304502210096bbb9d1f0596d69875646689e46f29485e8ceccacde9d0025db87fd96d3066902206d6de2dd69d965d28df3441b94c76e812384ab9297e69afe3480ee4031e1b2060121030f209b6ada5edb42c77fd2bc64ad650ae38314c8f451f3e36d80bc8e26f132cb00000000");
    ASSERT_EQ(output.transaction_id(), "3e3576eb02667fac284a5ecfcb25768969680cc4c597784602d0a33ba7c654b7");
    ASSERT_EQ(output.error(), Common::Proto::OK);

    // Successfully broadcasted: https://www.blockchain.com/explorer/transactions/btc/3e3576eb02667fac284a5ecfcb25768969680cc4c597784602d0a33ba7c654b7
}

TEST(BitcoinSigning, SignNftInscriptionCommit) {
    auto privateKey = parse_hex("e253373989199da27c48680e3a3fc0f648d50f9a727ef17a7fe6a4dc3b159129");
    auto fullAmount = 32400;
    auto minerFee = 1300;
    auto inscribeAmount = fullAmount - minerFee;
    auto txId = parse_hex("579590c3227253ad423b1e7e3c5b073b8a280d307c68aecd779df2600daa2f99");
    std::reverse(begin(txId), end(txId));

    // The inscribed image
    auto payload = parse_hex(nftInscriptionImageData);

    PrivateKey key(privateKey);
    auto pubKey = key.getPublicKey(TWPublicKeyTypeSECP256k1);
    auto utxoPubKeyHash = Hash::ripemd(Hash::sha256(pubKey.bytes));
    auto inputP2wpkh = TW::Bitcoin::Script::buildPayToWitnessPublicKeyHash(utxoPubKeyHash);
    auto outputInscribe = TW::Bitcoin::Script::buildOrdinalNftInscription("image/png", payload, pubKey.bytes);

    Proto::SigningInput input;
    input.set_is_it_brc_operation(true);
    input.add_private_key(key.bytes.data(), key.bytes.size());
    input.set_coin_type(TWCoinTypeBitcoin);

    auto& utxo0 = *input.add_utxo();
    utxo0.set_amount(fullAmount);
    utxo0.set_script(inputP2wpkh.bytes.data(), inputP2wpkh.bytes.size());
    utxo0.set_variant(Proto::TransactionVariant::P2WPKH);

    Proto::OutPoint out0;
    out0.set_index(0);
    out0.set_hash(txId.data(), txId.size());
    *utxo0.mutable_out_point() = out0;

    Proto::TransactionPlan plan;
    auto& utxo1 = *plan.add_utxos();
    utxo1.set_amount(inscribeAmount);
    utxo1.set_script(outputInscribe.script());
    utxo1.set_variant(Proto::TransactionVariant::NFTINSCRIPTION);

    *input.mutable_plan() = plan;
    Proto::SigningOutput output;

    ANY_SIGN(input, TWCoinTypeBitcoin);
    auto result = hex(output.encoded());
    ASSERT_EQ(hex(output.encoded()), "02000000000101992faa0d60f29d77cdae687c300d288a3b075b3c7e1e3b42ad537222c39095570000000000ffffffff017c790000000000002251202ac69a7e9dba801e9fcba826055917b84ca6fba4d51a29e47d478de603eedab602473044022054212984443ed4c66fc103d825bfd2da7baf2ab65d286e3c629b36b98cd7debd022050214cfe5d3b12a17aaaf1a196bfeb2f0ad15ffb320c4717eb7614162453e4fe0121030f209b6ada5edb42c77fd2bc64ad650ae38314c8f451f3e36d80bc8e26f132cb00000000");
    ASSERT_EQ(output.transaction_id(), "f1e708e5c5847339e16accf8716c14b33717c14d6fe68f9db36627cecbde7117");
    ASSERT_EQ(output.error(), Common::Proto::OK);

    // Successfully broadcasted: https://www.blockchain.com/explorer/transactions/btc/f1e708e5c5847339e16accf8716c14b33717c14d6fe68f9db36627cecbde7117
}

TEST(BitcoinSigning, SignNftInscriptionReveal) {
    auto privateKey = parse_hex("e253373989199da27c48680e3a3fc0f648d50f9a727ef17a7fe6a4dc3b159129");
    auto inscribeAmount = 31100;
    auto dustSatoshi = 546;
    auto txId = parse_hex("f1e708e5c5847339e16accf8716c14b33717c14d6fe68f9db36627cecbde7117");
    std::reverse(begin(txId), end(txId));

    // The inscribed image
    auto payload = parse_hex(nftInscriptionImageData);

    // The expected TX hex output
    auto expectedHex = parse_hex(nftInscriptionRawHex);

    PrivateKey key(privateKey);
    auto pubKey = key.getPublicKey(TWPublicKeyTypeSECP256k1);
    auto utxoPubKeyHash = Hash::ripemd(Hash::sha256(pubKey.bytes));
    auto inputInscribe = TW::Bitcoin::Script::buildOrdinalNftInscription("image/png", payload, pubKey.bytes);
    auto outputP2wpkh = TW::Bitcoin::Script::buildPayToWitnessPublicKeyHash(utxoPubKeyHash);

    Proto::SigningInput input;
    input.set_is_it_brc_operation(true);
    input.add_private_key(key.bytes.data(), key.bytes.size());
    input.set_coin_type(TWCoinTypeBitcoin);

    auto& utxo = *input.add_utxo();
    utxo.set_amount(inscribeAmount);
    utxo.set_script(inputInscribe.script());
    utxo.set_variant(Proto::TransactionVariant::NFTINSCRIPTION);
    utxo.set_spendingscript(inputInscribe.spendingscript());

    Proto::OutPoint out;
    out.set_index(0);
    out.set_hash(txId.data(), txId.size());
    *utxo.mutable_out_point() = out;

    Proto::TransactionPlan plan;
    auto& utxo1 = *plan.add_utxos();
    utxo1.set_amount(dustSatoshi);
    utxo1.set_script(outputP2wpkh.bytes.data(), outputP2wpkh.bytes.size());
    utxo1.set_variant(Proto::TransactionVariant::P2WPKH);

    *input.mutable_plan() = plan;
    Proto::SigningOutput output;

    ANY_SIGN(input, TWCoinTypeBitcoin);
    auto result = hex(output.encoded());
    ASSERT_EQ(result.substr(0, 164), expectedHex.substr(0, 164));
    ASSERT_EQ(result.substr(292, result.size() - 292), expectedHex.substr(292, result.size() - 292));
    ASSERT_EQ(output.transaction_id(), "173f8350b722243d44cc8db5584de76b432eb6d0888d9e66e662db51584f44ac");
    ASSERT_EQ(output.error(), Common::Proto::OK);

    // Successfully broadcasted: https://www.blockchain.com/explorer/transactions/btc/173f8350b722243d44cc8db5584de76b432eb6d0888d9e66e662db51584f44ac
}

TEST(BitcoinSigning, SignP2PKH) {
    auto input = buildInputP2PKH();

    {
        // test plan (but do not reuse plan result)
        auto plan = TransactionBuilder::plan(input);
        EXPECT_TRUE(verifyPlan(plan, {625'000'000}, 335'790'000, 226));
    }

    // Sign
    auto result = TransactionSigner<Transaction, TransactionBuilder>::sign(input);

    ASSERT_TRUE(result) << std::to_string(result.error());
    auto signedTx = result.payload();

    Data serialized;
    signedTx.encode(serialized);
    EXPECT_EQ(getEncodedTxSize(signedTx), (EncodedTxSize{228, 225, 226}));
    EXPECT_TRUE(validateEstimatedSize(signedTx, -1, 1));
    ASSERT_EQ(hex(serialized), // printed using prettyPrintTransaction
        "01000000" // version
        "01" // inputs
            "fff7f7881a8099afa6940d42d1e7f6362bec38171ea3edf433541db4e4ad969f"  "00000000"  "6a"  "47304402202819d70d4bec472113a1392cadc0860a7a1b34ea0869abb4bdce3290c3aba086022023eff75f410ad19cdbe6c6a017362bd554ce5fb906c13534ddc306be117ad30a012103c9f4836b9a4f77fc0d81f7bcb01b7f1b35916864b9476c241ce9fc198bd25432"  "ffffffff"
        "02" // outputs
            "b0bf031400000000"  "19"  "76a914769bdff96a02f9135a1d19b749db6a78fe07dc9088ac"
            "aefd3c1100000000"  "19"  "76a9149e089b6889e032d46e3b915a3392edfd616fb1c488ac"
        "00000000" // nLockTime
    );
}

TEST(BitcoinSigning, SignP2PKH_NegativeMissingKey) {
    auto input = buildInputP2PKH(true);

    {
        // test plan (but do not reuse plan result). Plan works even with missing keys.
        auto plan = TransactionBuilder::plan(input);
        EXPECT_TRUE(verifyPlan(plan, {625'000'000}, 335'790'000, 226));
    }

    // Sign
    auto result = TransactionSigner<Transaction, TransactionBuilder>::sign(input);

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), Common::Proto::Error_missing_private_key);
}

TEST(BitcoinSigning, EncodeP2WPKH) {
    auto unsignedTx = Transaction(1, 0x11);

    auto hash0 = parse_hex("fff7f7881a8099afa6940d42d1e7f6362bec38171ea3edf433541db4e4ad969f");
    auto outpoint0 = TW::Bitcoin::OutPoint(hash0, 0);
    unsignedTx.inputs.emplace_back(outpoint0, Script(), 0xffffffee);

    auto hash1 = parse_hex("ef51e1b804cc89d182d279655c3aa89e815b1b309fe287d9b2b55d57b90ec68a");
    auto outpoint1 = TW::Bitcoin::OutPoint(hash1, 1);
    unsignedTx.inputs.emplace_back(outpoint1, Script(), UINT32_MAX);

    auto outScript0 = Script(parse_hex("76a9148280b37df378db99f66f85c95a783a76ac7a6d5988ac"));
    unsignedTx.outputs.emplace_back(112340000, outScript0);

    auto outScript1 = Script(parse_hex("76a9143bde42dbee7e4dbe6a21b2d50ce2f0167faa815988ac"));
    unsignedTx.outputs.emplace_back(223450000, outScript1);

    Data unsignedData;
    unsignedTx.encode(unsignedData, Transaction::SegwitFormatMode::Segwit);
    ASSERT_EQ(unsignedData.size(), 164ul);
    ASSERT_EQ(hex(unsignedData),
        "01000000" // version
        "0001" // marker & flag
        "02" // inputs
            "fff7f7881a8099afa6940d42d1e7f6362bec38171ea3edf433541db4e4ad969f"  "00000000"  "00"  ""  "eeffffff"
            "ef51e1b804cc89d182d279655c3aa89e815b1b309fe287d9b2b55d57b90ec68a"  "01000000"  "00"  ""  "ffffffff"
        "02" // outputs
            "202cb20600000000"  "19"  "76a9148280b37df378db99f66f85c95a783a76ac7a6d5988ac"
            "9093510d00000000"  "19"  "76a9143bde42dbee7e4dbe6a21b2d50ce2f0167faa815988ac"
        // witness
            "00"
            "00"
        "11000000" // nLockTime
    );
}

TEST(BitcoinSigning, SignP2WPKH_Bip143) {
    // https://github.com/bitcoin/bips/blob/master/bip-0143.mediawiki#native-p2wpkh

    SigningInput input;
    input.hashType = TWBitcoinSigHashTypeAll;
    const auto amount = 112340000; // 0x06B22C20
    input.amount = amount;
    input.totalAmount = amount;
    input.byteFee = 20; // not relevant
    input.toAddress = "1Cu32FVupVCgHkMMRJdYJugxwo2Aprgk7H";
    input.changeAddress = "16TZ8J6Q5iZKBWizWzFAYnrsaox5Z5aBRV";

    const auto hash0 = parse_hex("fff7f7881a8099afa6940d42d1e7f6362bec38171ea3edf433541db4e4ad969f");
    const auto utxoKey0 = PrivateKey(parse_hex("bbc27228ddcb9209d7fd6f36b02f7dfa6252af40bb2f1cbc7a557da8027ff866"));
    const auto pubKey0 = utxoKey0.getPublicKey(TWPublicKeyTypeSECP256k1);
    EXPECT_EQ(hex(pubKey0.bytes), "03c9f4836b9a4f77fc0d81f7bcb01b7f1b35916864b9476c241ce9fc198bd25432");

    const auto utxo0Script = Script::buildPayToPublicKey(pubKey0.bytes);
    Data key2;
    utxo0Script.matchPayToPublicKey(key2);
    EXPECT_EQ(hex(key2), hex(pubKey0.bytes));
    input.privateKeys.push_back(utxoKey0);

    const auto hash1 = parse_hex("ef51e1b804cc89d182d279655c3aa89e815b1b309fe287d9b2b55d57b90ec68a");
    const auto utxoKey1 = PrivateKey(parse_hex("619c335025c7f4012e556c2a58b2506e30b8511b53ade95ea316fd8c3286feb9"));
    const auto pubKey1 = utxoKey1.getPublicKey(TWPublicKeyTypeSECP256k1);
    EXPECT_EQ(hex(pubKey1.bytes), "025476c2e83188368da1ff3e292e7acafcdb3566bb0ad253f62fc70f07aeee6357");
    const auto utxoPubkeyHash1 = Hash::ripemd(Hash::sha256(pubKey1.bytes));
    EXPECT_EQ(hex(utxoPubkeyHash1), "1d0f172a0ecb48aee1be1f2687d2963ae33f71a1");
    input.privateKeys.push_back(utxoKey1);
    input.lockTime = 0x11;

    UTXO utxo0;
    utxo0.script = utxo0Script;
    utxo0.amount = 1000000; // note: this amount is not specified in the test
    utxo0.outPoint = OutPoint(hash0, 0, 0xffffffee);
    input.utxos.push_back(utxo0);

    UTXO utxo1;
    auto utxo1Script = Script::buildPayToV0WitnessProgram(utxoPubkeyHash1);
    utxo1.script = utxo1Script;
    utxo1.amount = 600000000; // 0x23C34600 0046c323
    utxo1.outPoint = OutPoint(hash1, 1, UINT32_MAX);
    input.utxos.push_back(utxo1);

    // Set plan to force both UTXOs and exact output amounts
    TransactionPlan plan;
    plan.amount = amount;
    plan.availableAmount = 600000000 + 1000000;
    plan.fee = 265210000;    // very large, the amounts specified (in1, out0, out1) are not consistent/realistic
    plan.change = 223450000; // 0x0d519390
    plan.branchId = {0};
    plan.utxos.push_back(utxo0);
    plan.utxos.push_back(utxo1);
    input.plan = plan;

    // Sign
    auto result = TransactionSigner<Transaction, TransactionBuilder>::sign(input);

    ASSERT_TRUE(result) << std::to_string(result.error());
    const auto signedTx = result.payload();

    Data serialized;
    signedTx.encode(serialized);
    EXPECT_EQ(getEncodedTxSize(signedTx), (EncodedTxSize{343, 233, 261}));
    // expected in one string for easy comparison/copy:
    ASSERT_EQ(hex(serialized), "01000000000102fff7f7881a8099afa6940d42d1e7f6362bec38171ea3edf433541db4e4ad969f00000000494830450221008b9d1dc26ba6a9cb62127b02742fa9d754cd3bebf337f7a55d114c8e5cdd30be022040529b194ba3f9281a99f2b1c0a19c0489bc22ede944ccf4ecbab4cc618ef3ed01eeffffffef51e1b804cc89d182d279655c3aa89e815b1b309fe287d9b2b55d57b90ec68a0100000000ffffffff02202cb206000000001976a9148280b37df378db99f66f85c95a783a76ac7a6d5988ac9093510d000000001976a9143bde42dbee7e4dbe6a21b2d50ce2f0167faa815988ac000247304402203609e17b84f6a7d30c80bfa610b5b4542f32a8a0d5447a12fb1366d7f01cc44a0220573a954c4518331561406f90300e8f3358f51928d43c212a8caed02de67eebee0121025476c2e83188368da1ff3e292e7acafcdb3566bb0ad253f62fc70f07aeee635711000000");
    // expected in structured format:
    ASSERT_EQ(hex(serialized), // printed using prettyPrintTransaction
        "01000000" // version
        "0001" // marker & flag
        "02" // inputs
            "fff7f7881a8099afa6940d42d1e7f6362bec38171ea3edf433541db4e4ad969f"  "00000000"  "49"  "4830450221008b9d1dc26ba6a9cb62127b02742fa9d754cd3bebf337f7a55d114c8e5cdd30be022040529b194ba3f9281a99f2b1c0a19c0489bc22ede944ccf4ecbab4cc618ef3ed01"  "eeffffff"
            "ef51e1b804cc89d182d279655c3aa89e815b1b309fe287d9b2b55d57b90ec68a"  "01000000"  "00"  ""  "ffffffff"
        "02" // outputs
            "202cb20600000000"  "19"  "76a9148280b37df378db99f66f85c95a783a76ac7a6d5988ac"
            "9093510d00000000"  "19"  "76a9143bde42dbee7e4dbe6a21b2d50ce2f0167faa815988ac"
        // witness
            "00"
            "02"
                "47"  "304402203609e17b84f6a7d30c80bfa610b5b4542f32a8a0d5447a12fb1366d7f01cc44a0220573a954c4518331561406f90300e8f3358f51928d43c212a8caed02de67eebee01"
                "21"  "025476c2e83188368da1ff3e292e7acafcdb3566bb0ad253f62fc70f07aeee6357"
        "11000000" // nLockTime
    );
}

SigningInput buildInputP2WPKH(int64_t amount, TWBitcoinSigHashType hashType, int64_t utxo0Amount, int64_t utxo1Amount, bool useMaxAmount = false) {
    auto hash0 = parse_hex("fff7f7881a8099afa6940d42d1e7f6362bec38171ea3edf433541db4e4ad969f");
    auto hash1 = parse_hex("ef51e1b804cc89d182d279655c3aa89e815b1b309fe287d9b2b55d57b90ec68a");

    // Setup input
    SigningInput input;
    input.hashType = hashType;
    input.amount = amount;
    input.totalAmount = amount;
    input.useMaxAmount = useMaxAmount;
    input.byteFee = 1;
    input.toAddress = "1Bp9U1ogV3A14FMvKbRJms7ctyso4Z4Tcx";
    input.changeAddress = "1FQc5LdgGHMHEN9nwkjmz6tWkxhPpxBvBU";
    input.coinType = TWCoinTypeBitcoin;

    auto utxoKey0 = PrivateKey(parse_hex("bbc27228ddcb9209d7fd6f36b02f7dfa6252af40bb2f1cbc7a557da8027ff866"));
    auto pubKey0 = utxoKey0.getPublicKey(TWPublicKeyTypeSECP256k1);
    auto utxoPubkeyHash0 = Hash::ripemd(Hash::sha256(pubKey0.bytes));
    assert(hex(utxoPubkeyHash0) == "b7cd046b6d522a3d61dbcb5235c0e9cc97265457");
    input.privateKeys.push_back(utxoKey0);

    auto utxoKey1 = PrivateKey(parse_hex("619c335025c7f4012e556c2a58b2506e30b8511b53ade95ea316fd8c3286feb9"));
    auto pubKey1 = utxoKey1.getPublicKey(TWPublicKeyTypeSECP256k1);
    auto utxoPubkeyHash1 = Hash::ripemd(Hash::sha256(pubKey1.bytes));
    assert(hex(utxoPubkeyHash1) == "1d0f172a0ecb48aee1be1f2687d2963ae33f71a1");
    input.privateKeys.push_back(utxoKey1);

    auto scriptPub1 = Script(parse_hex("0014"
                                       "1d0f172a0ecb48aee1be1f2687d2963ae33f71a1"));
    Data scriptHash;
    scriptPub1.matchPayToWitnessPublicKeyHash(scriptHash);
    auto scriptHashHex = hex(scriptHash);
    assert(scriptHashHex == "1d0f172a0ecb48aee1be1f2687d2963ae33f71a1");

    auto redeemScript = Script::buildPayToPublicKeyHash(parse_hex("1d0f172a0ecb48aee1be1f2687d2963ae33f71a1"));
    input.scripts[scriptHashHex] = redeemScript;

    UTXO utxo0;
    utxo0.script = Script(parse_hex("2103c9f4836b9a4f77fc0d81f7bcb01b7f1b35916864b9476c241ce9fc198bd25432ac"));
    utxo0.amount = utxo0Amount;
    utxo0.outPoint = OutPoint(hash0, 0, UINT32_MAX);
    input.utxos.push_back(utxo0);

    UTXO utxo1;
    utxo1.script = Script(parse_hex("0014"
                                    "1d0f172a0ecb48aee1be1f2687d2963ae33f71a1"));
    utxo1.amount = utxo1Amount;
    utxo1.outPoint = OutPoint(hash1, 1, UINT32_MAX);
    input.utxos.push_back(utxo1);

    return input;
}

TEST(BitcoinSigning, SignP2WPKH) {
    auto input = buildInputP2WPKH(335'790'000, TWBitcoinSigHashTypeAll, 625'000'000, 600'000'000);

    {
        // test plan (but do not reuse plan result)
        auto plan = TransactionBuilder::plan(input);
        EXPECT_TRUE(verifyPlan(plan, {625'000'000}, 335'790'000, 192));
    }

    // Signs
    auto result = TransactionSigner<Transaction, TransactionBuilder>::sign(input);

    ASSERT_TRUE(result) << std::to_string(result.error());
    auto signedTx = result.payload();

    Data serialized;
    signedTx.encode(serialized);
    EXPECT_EQ(getEncodedTxSize(signedTx), (EncodedTxSize{195, 192, 193}));
    EXPECT_EQ(serialized.size(), 192ul);
    EXPECT_TRUE(validateEstimatedSize(signedTx, -1, 1));
    ASSERT_EQ(hex(serialized), // printed using prettyPrintTransaction
        "01000000" // version
        "01" // inputs
            "fff7f7881a8099afa6940d42d1e7f6362bec38171ea3edf433541db4e4ad969f"  "00000000"  "49"  "483045022100c327babdd370f0fc5b24cf920736446bf7d9c5660e4a5f7df432386fd652fe280220269c4fc3690c1c248e50c8bf2435c20b4ef00f308b403575f4437f862a91c53a01"  "ffffffff"
        "02" // outputs
            "b0bf031400000000"  "19"  "76a914769bdff96a02f9135a1d19b749db6a78fe07dc9088ac"
            "d0fd3c1100000000"  "19"  "76a9149e089b6889e032d46e3b915a3392edfd616fb1c488ac"
        "00000000" // nLockTime
    );

    {
        // Non-segwit encoded, for comparison
        Data serialized_;
        signedTx.encode(serialized_, Transaction::SegwitFormatMode::NonSegwit);
        EXPECT_EQ(getEncodedTxSize(signedTx), (EncodedTxSize{195, 192, 193}));
        EXPECT_EQ(serialized_.size(), 192ul);
        ASSERT_EQ(hex(serialized_), // printed using prettyPrintTransaction
            "01000000" // version
            "01" // inputs
                "fff7f7881a8099afa6940d42d1e7f6362bec38171ea3edf433541db4e4ad969f"  "00000000"  "49"  "483045022100c327babdd370f0fc5b24cf920736446bf7d9c5660e4a5f7df432386fd652fe280220269c4fc3690c1c248e50c8bf2435c20b4ef00f308b403575f4437f862a91c53a01"  "ffffffff"
            "02" // outputs
                "b0bf031400000000"  "19"  "76a914769bdff96a02f9135a1d19b749db6a78fe07dc9088ac"
                "d0fd3c1100000000"  "19"  "76a9149e089b6889e032d46e3b915a3392edfd616fb1c488ac"
            "00000000" // nLockTime
        );
    }
}

TEST(BitcoinSigning, SignP2WPKH_HashSingle_TwoInput) {
    auto input = buildInputP2WPKH(335'790'000, TWBitcoinSigHashTypeSingle, 210'000'000, 210'000'000);

    {
        // test plan (but do not reuse plan result)
        auto plan = TransactionBuilder::plan(input);
        EXPECT_TRUE(verifyPlan(plan, {210'000'000, 210'000'000}, 335'790'000, 261));
    }

    // Sign
    auto result = TransactionSigner<Transaction, TransactionBuilder>::sign(input);

    ASSERT_TRUE(result) << std::to_string(result.error());
    auto signedTx = result.payload();

    Data serialized;
    signedTx.encode(serialized);
    EXPECT_EQ(getEncodedTxSize(signedTx), (EncodedTxSize{343, 233, 261}));
    EXPECT_TRUE(validateEstimatedSize(signedTx, -1, 1));
    ASSERT_EQ(hex(serialized), // printed using prettyPrintTransaction
        "01000000" // version
        "0001" // marker & flag
        "02" // inputs
            "fff7f7881a8099afa6940d42d1e7f6362bec38171ea3edf433541db4e4ad969f"  "00000000"  "49"  "483045022100fd8591c3611a07b55f509ec850534c7a9c49713c9b8fa0e844ea06c2e65e19d702205e3806676192e790bc93dd4c28e937c4bf97b15f189158ba1a30d7ecff5ee75503"  "ffffffff"
            "ef51e1b804cc89d182d279655c3aa89e815b1b309fe287d9b2b55d57b90ec68a"  "01000000"  "00"  ""  "ffffffff"
        "02" // outputs
            "b0bf031400000000"  "19"  "76a914769bdff96a02f9135a1d19b749db6a78fe07dc9088ac"
            "4bf0040500000000"  "19"  "76a9149e089b6889e032d46e3b915a3392edfd616fb1c488ac"
        // witness
            "00"
            "02"  "47"  "30440220096d20c7e92f991c2bf38dc28118feb34019ae74ec1c17179b28cb041de7517402204594f46a911f24bdc7109ca192e6860ebf2f3a0087579b3c128d5ce0cd5ed46803"  "21"  "025476c2e83188368da1ff3e292e7acafcdb3566bb0ad253f62fc70f07aeee6357"
        "00000000" // nLockTime
    );
}

TEST(BitcoinSigning, SignP2WPKH_HashAnyoneCanPay_TwoInput) {
    auto input = buildInputP2WPKH(335'790'000, TWBitcoinSigHashTypeAnyoneCanPay, 210'000'000, 210'000'000);

    {
        // test plan (but do not reuse plan result)
        auto plan = TransactionBuilder::plan(input);
        EXPECT_TRUE(verifyPlan(plan, {210'000'000, 210'000'000}, 335'790'000, 261));
    }

    // Sign
    auto result = TransactionSigner<Transaction, TransactionBuilder>::sign(input);

    ASSERT_TRUE(result) << std::to_string(result.error());
    auto signedTx = result.payload();

    Data serialized;
    signedTx.encode(serialized);
    EXPECT_EQ(getEncodedTxSize(signedTx), (EncodedTxSize{344, 233, 261}));
    EXPECT_TRUE(validateEstimatedSize(signedTx, -1, 1));
    ASSERT_EQ(hex(serialized), // printed using prettyPrintTransaction
        "01000000" // version
        "0001" // marker & flag
        "02" // inputs
            "fff7f7881a8099afa6940d42d1e7f6362bec38171ea3edf433541db4e4ad969f"  "00000000"  "49"  "483045022100e21fb2f1cfd59bdb3703fd45db38fd680d0c06e5d0be86fb7dc233c07ee7ab2f02207367220a73e43df4352a6831f6f31d8dc172c83c9f613a9caf679f0f15621c5e80"  "ffffffff"
            "ef51e1b804cc89d182d279655c3aa89e815b1b309fe287d9b2b55d57b90ec68a"  "01000000"  "00"  ""  "ffffffff"
        "02" // outputs
            "b0bf031400000000"  "19"  "76a914769bdff96a02f9135a1d19b749db6a78fe07dc9088ac"
            "4bf0040500000000"  "19"  "76a9149e089b6889e032d46e3b915a3392edfd616fb1c488ac"
        // witness
            "00"
            "02"  "48"  "304502210095f9cc913d2f0892b953f2380112533e8930b67c53e00a7bbd7a01d547156adc022026efe3a684aa7432a00a919dbf81b63e635fb92d3149453e95b4a7ccea59f7c480"  "21"  "025476c2e83188368da1ff3e292e7acafcdb3566bb0ad253f62fc70f07aeee6357"
        "00000000" // nLockTime
    );
}

TEST(BitcoinSigning, SignP2WPKH_MaxAmount) {
    auto input = buildInputP2WPKH(1'000, TWBitcoinSigHashTypeAll, 625'000'000, 600'000'000, true);
    input.totalAmount = 1'224'999'773;
    {
        // test plan (but do not reuse plan result)
        auto plan = TransactionBuilder::plan(input);
        EXPECT_TRUE(verifyPlan(plan, {625'000'000, 600'000'000}, 1'224'999'773, 227));
    }

    // Sign
    auto result = TransactionSigner<Transaction, TransactionBuilder>::sign(input);

    ASSERT_TRUE(result) << std::to_string(result.error());
    auto signedTx = result.payload();

    Data serialized;
    signedTx.encode(serialized);
    EXPECT_EQ(getEncodedTxSize(signedTx), (EncodedTxSize{310, 199, 227}));
    EXPECT_TRUE(validateEstimatedSize(signedTx, -1, 1));
    ASSERT_EQ(hex(serialized), // printed using prettyPrintTransaction
        "01000000" // version
        "0001" // marker & flag
        "02" // inputs
            "fff7f7881a8099afa6940d42d1e7f6362bec38171ea3edf433541db4e4ad969f"  "00000000"  "49"  "483045022100a8b3c1619e985923994e80efdc0be0eac12f2419e11ce5e4286a0a5ac27c775d02205d6feee85ffe19ae0835cba1562beb3beb172107cd02ac4caf24a8be3749811f01"  "ffffffff"
            "ef51e1b804cc89d182d279655c3aa89e815b1b309fe287d9b2b55d57b90ec68a"  "01000000"  "00"  ""  "ffffffff"
        "01" // outputs
            "5d03044900000000"  "19"  "76a914769bdff96a02f9135a1d19b749db6a78fe07dc9088ac"
        // witness
            "00"
            "02"  "48"  "3045022100db1199de92f6fb638a0ba706d13ec686bb01138a254dec2c397616cd74bad30e02200d7286d6d2d4e00d145955bf3d3b848b03c0d1eef8899e4645687a3035d7def401"  "21"  "025476c2e83188368da1ff3e292e7acafcdb3566bb0ad253f62fc70f07aeee6357"
        "00000000" // nLockTime
    );
}

TEST(BitcoinSigning, EncodeP2WSH) {
    auto unsignedTx = Transaction(1);

    auto outpoint0 = OutPoint(parse_hex("0001000000000000000000000000000000000000000000000000000000000000"), 0);
    unsignedTx.inputs.emplace_back(outpoint0, Script(), UINT32_MAX);

    auto outScript0 = Script(parse_hex("76a9144c9c3dfac4207d5d8cb89df5722cb3d712385e3f88ac"));
    unsignedTx.outputs.emplace_back(1000, outScript0);

    Data unsignedData;
    unsignedTx.encode(unsignedData, Transaction::SegwitFormatMode::NonSegwit);
    ASSERT_EQ(hex(unsignedData),
        "01000000" // version
        "01" // inputs
            "0001000000000000000000000000000000000000000000000000000000000000"  "00000000"  "00"  ""  "ffffffff"
        "01" // outputs
            "e803000000000000"  "19"  "76a9144c9c3dfac4207d5d8cb89df5722cb3d712385e3f88ac"
        "00000000" // nLockTime
    );
}

SigningInput buildInputP2WSH(enum TWBitcoinSigHashType hashType, bool omitScript = false, bool omitKeys = false) {
    SigningInput input;
    input.hashType = hashType;
    input.amount = 1000;
    input.totalAmount = 1000;
    input.byteFee = 1;
    input.toAddress = "1Bp9U1ogV3A14FMvKbRJms7ctyso4Z4Tcx";
    input.changeAddress = "1FQc5LdgGHMHEN9nwkjmz6tWkxhPpxBvBU";

    if (!omitKeys) {
        auto utxoKey0 = PrivateKey(parse_hex("ed00a0841cd53aedf89b0c616742d1d2a930f8ae2b0fb514765a17bb62c7521a"));
        input.privateKeys.push_back(utxoKey0);

        auto utxoKey1 = PrivateKey(parse_hex("619c335025c7f4012e556c2a58b2506e30b8511b53ade95ea316fd8c3286feb9"));
        input.privateKeys.push_back(utxoKey1);
    }

    if (!omitScript) {
        auto redeemScript = Script(parse_hex("2103596d3451025c19dbbdeb932d6bf8bfb4ad499b95b6f88db8899efac102e5fc71ac"));
        auto scriptHash = "593128f9f90e38b706c18623151e37d2da05c229";
        input.scripts[scriptHash] = redeemScript;
    }

    UTXO utxo0;
    auto p2wsh = Script::buildPayToWitnessScriptHash(parse_hex("ff25429251b5a84f452230a3c75fd886b7fc5a7865ce4a7bb7a9d7c5be6da3db"));
    utxo0.script = p2wsh;
    utxo0.amount = 1226;
    auto hash0 = parse_hex("0001000000000000000000000000000000000000000000000000000000000000");
    utxo0.outPoint = OutPoint(hash0, 0, UINT32_MAX);
    input.utxos.push_back(utxo0);

    return input;
}

TEST(BitcoinSigning, SignP2WSH) {
    // Setup input
    const auto input = buildInputP2WSH(hashTypeForCoin(TWCoinTypeBitcoin));

    {
        // test plan (but do not reuse plan result)
        auto plan = TransactionBuilder::plan(input);
        EXPECT_TRUE(verifyPlan(plan, {1'226}, 1'000, 147));
    }

    // Sign
    auto result = TransactionSigner<Transaction, TransactionBuilder>::sign(input);

    ASSERT_TRUE(result) << std::to_string(result.error());
    auto signedTx = result.payload();

    Data serialized;
    signedTx.encode(serialized);
    EXPECT_EQ(getEncodedTxSize(signedTx), (EncodedTxSize{231, 119, 147}));
    EXPECT_TRUE(validateEstimatedSize(signedTx, -1, 1));
    ASSERT_EQ(hex(serialized), // printed using prettyPrintTransaction
        "01000000" // version
        "0001" // marker & flag
        "01" // inputs
            "0001000000000000000000000000000000000000000000000000000000000000"  "00000000"  "00"  ""  "ffffffff"
        "02" // outputs
            "e803000000000000"  "19"  "76a914769bdff96a02f9135a1d19b749db6a78fe07dc9088ac"
            "4f00000000000000"  "19"  "76a9149e089b6889e032d46e3b915a3392edfd616fb1c488ac"
        // witness
            "02"  "48"  "30450221009eefc1befe96158f82b74e6804f1f713768c6172636ca11fcc975c316ea86f75022057914c48bc24f717498b851a47a2926f96242e3943ebdf08d5a97a499efc8b9001"  "23"  "2103596d3451025c19dbbdeb932d6bf8bfb4ad499b95b6f88db8899efac102e5fc71ac"
        "00000000" // nLockTime
    );
}

TEST(BitcoinSigning, SignP2WSH_HashNone) {
    // Setup input
    const auto input = buildInputP2WSH(TWBitcoinSigHashTypeNone);

    {
        // test plan (but do not reuse plan result)
        auto plan = TransactionBuilder::plan(input);
        EXPECT_TRUE(verifyPlan(plan, {1'226}, 1'000, 147));
    }

    // Sign
    auto result = TransactionSigner<Transaction, TransactionBuilder>::sign(input);

    ASSERT_TRUE(result) << std::to_string(result.error());
    auto signedTx = result.payload();

    Data serialized;
    signedTx.encode(serialized);
    EXPECT_EQ(getEncodedTxSize(signedTx), (EncodedTxSize{231, 119, 147}));
    EXPECT_TRUE(validateEstimatedSize(signedTx, -1, 1));
    ASSERT_EQ(hex(serialized), // printed using prettyPrintTransaction
        "01000000" // version
        "0001" // marker & flag
        "01" // inputs
            "0001000000000000000000000000000000000000000000000000000000000000"  "00000000"  "00"  ""  "ffffffff"
        "02" // outputs
            "e803000000000000"  "19"  "76a914769bdff96a02f9135a1d19b749db6a78fe07dc9088ac"
            "4f00000000000000"  "19"  "76a9149e089b6889e032d46e3b915a3392edfd616fb1c488ac"
        // witness
            "02"  "48"  "3045022100caa585732cfc50226a90834a306d23d5d2ab1e94af2c66136a637e3d9bad3688022069028750908e53a663bb1f434fd655bcc0cf8d394c6fa1fd5a4983790135722e02"  "23"  "2103596d3451025c19dbbdeb932d6bf8bfb4ad499b95b6f88db8899efac102e5fc71ac"
        "00000000" // nLockTime
    );
}

TEST(BitcoinSigning, SignP2WSH_HashSingle) {
    // Setup input
    const auto input = buildInputP2WSH(TWBitcoinSigHashTypeSingle);

    {
        // test plan (but do not reuse plan result)
        auto plan = TransactionBuilder::plan(input);
        EXPECT_TRUE(verifyPlan(plan, {1'226}, 1'000, 147));
    }

    // Sign
    auto result = TransactionSigner<Transaction, TransactionBuilder>::sign(input);

    ASSERT_TRUE(result) << std::to_string(result.error());
    auto signedTx = result.payload();

    Data serialized;
    signedTx.encode(serialized);
    EXPECT_EQ(getEncodedTxSize(signedTx), (EncodedTxSize{230, 119, 147}));
    EXPECT_TRUE(validateEstimatedSize(signedTx, -1, 1));
    ASSERT_EQ(hex(serialized), // printed using prettyPrintTransaction
        "01000000" // version
        "0001" // marker & flag
        "01" // inputs
            "0001000000000000000000000000000000000000000000000000000000000000"  "00000000"  "00"  ""  "ffffffff"
        "02" // outputs
            "e803000000000000"  "19"  "76a914769bdff96a02f9135a1d19b749db6a78fe07dc9088ac"
            "4f00000000000000"  "19"  "76a9149e089b6889e032d46e3b915a3392edfd616fb1c488ac"
        // witness
            "02"  "47"  "304402201ba80b2c48fe82915297dc9782ae2141e40263001fafd21b02c04a092503f01e0220666d6c63475c6c52abd09371c200ac319bcf4a7c72eb3782e95790f5c847f0b903"  "23"  "2103596d3451025c19dbbdeb932d6bf8bfb4ad499b95b6f88db8899efac102e5fc71ac"
        "00000000" // nLockTime
    );
}

TEST(BitcoinSigning, SignP2WSH_HashAnyoneCanPay) {
    // Setup input
    const auto input = buildInputP2WSH(TWBitcoinSigHashTypeAnyoneCanPay);

    {
        // test plan (but do not reuse plan result)
        auto plan = TransactionBuilder::plan(input);
        EXPECT_TRUE(verifyPlan(plan, {1'226}, 1'000, 147));
    }

    // Sign
    auto result = TransactionSigner<Transaction, TransactionBuilder>::sign(input);

    ASSERT_TRUE(result) << std::to_string(result.error());
    auto signedTx = result.payload();

    Data serialized;
    signedTx.encode(serialized);
    EXPECT_EQ(serialized.size(), 231ul);
    EXPECT_EQ(getEncodedTxSize(signedTx), (EncodedTxSize{231, 119, 147}));
    EXPECT_TRUE(validateEstimatedSize(signedTx, -1, 1));
    ASSERT_EQ(hex(serialized), // printed using prettyPrintTransaction
        "01000000" // version
        "0001" // marker & flag
        "01" // inputs
            "0001000000000000000000000000000000000000000000000000000000000000"  "00000000"  "00"  ""  "ffffffff"
        "02" // outputs
            "e803000000000000"  "19"  "76a914769bdff96a02f9135a1d19b749db6a78fe07dc9088ac"
            "4f00000000000000"  "19"  "76a9149e089b6889e032d46e3b915a3392edfd616fb1c488ac"
        // witness
            "02"  "48"  "3045022100d14699fc9b7337768bcd1430098d279cfaf05f6abfa75dd542da2dc038ae1700022063f0751c08796c086ac23b39c25f4320f432092e0c11bec46af0723cc4f55a3980"  "23"  "2103596d3451025c19dbbdeb932d6bf8bfb4ad499b95b6f88db8899efac102e5fc71ac"
        "00000000" // nLockTime
    );
}

TEST(BitcoinSigning, SignP2WSH_NegativeMissingScript) {
    const auto input = buildInputP2WSH(TWBitcoinSigHashTypeAll, true);

    {
        // test plan (but do not reuse plan result)
        auto plan = TransactionBuilder::plan(input);
        EXPECT_TRUE(verifyPlan(plan, {1'226}, 1'000, 174));
    }

    // Sign
    auto result = TransactionSigner<Transaction, TransactionBuilder>::sign(input);

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), Common::Proto::Error_script_redeem);
}

TEST(BitcoinSigning, SignP2WSH_NegativeMissingKeys) {
    const auto input = buildInputP2WSH(TWBitcoinSigHashTypeAll, false, true);

    {
        // test plan (but do not reuse plan result). Plan works even with missing keys.
        auto plan = TransactionBuilder::plan(input);
        EXPECT_TRUE(verifyPlan(plan, {1'226}, 1'000, 147));
    }

    // Sign
    auto result = TransactionSigner<Transaction, TransactionBuilder>::sign(input);

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), Common::Proto::Error_missing_private_key);
}

TEST(BitcoinSigning, SignP2WSH_NegativePlanWithError) {
    // Setup input
    auto input = buildInputP2WSH(TWBitcoinSigHashTypeAll);
    input.plan = TransactionBuilder::plan(input);
    input.plan->error = Common::Proto::Error_missing_input_utxos;

    // Sign
    auto result = TransactionSigner<Transaction, TransactionBuilder>::sign(input);

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), Common::Proto::Error_missing_input_utxos);
}

TEST(BitcoinSigning, SignP2WSH_NegativeNoUTXOs) {
    // Setup input
    auto input = buildInputP2WSH(TWBitcoinSigHashTypeAll);
    input.utxos.clear();
    ASSERT_FALSE(input.plan.has_value());

    // Sign
    auto result = TransactionSigner<Transaction, TransactionBuilder>::sign(input);

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), Common::Proto::Error_missing_input_utxos);
}

TEST(BitcoinSigning, SignP2WSH_NegativePlanWithNoUTXOs) {
    // Setup input
    auto input = buildInputP2WSH(TWBitcoinSigHashTypeAll);
    input.plan = TransactionBuilder::plan(input);
    input.plan->utxos.clear();

    // Sign
    auto result = TransactionSigner<Transaction, TransactionBuilder>::sign(input);

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), Common::Proto::Error_missing_input_utxos);
}

TEST(BitcoinSigning, EncodeP2SH_P2WPKH) {
    auto unsignedTx = Transaction(1, 0x492);

    auto outpoint0 = OutPoint(parse_hex("db6b1b20aa0fd7b23880be2ecbd4a98130974cf4748fb66092ac4d3ceb1a5477"), 1);
    unsignedTx.inputs.emplace_back(outpoint0, Script(), 0xfffffffe);

    auto outScript0 = Script(parse_hex("76a914a457b684d7f0d539a46a45bbc043f35b59d0d96388ac"));
    unsignedTx.outputs.emplace_back(199'996'600, outScript0);

    auto outScript1 = Script(parse_hex("76a914fd270b1ee6abcaea97fea7ad0402e8bd8ad6d77c88ac"));
    unsignedTx.outputs.emplace_back(800'000'000, outScript1);

    Data unsignedData;
    unsignedTx.encode(unsignedData, Transaction::SegwitFormatMode::NonSegwit);
    ASSERT_EQ(hex(unsignedData),
        "01000000" // version
        "01" // inputs
            "db6b1b20aa0fd7b23880be2ecbd4a98130974cf4748fb66092ac4d3ceb1a5477"  "01000000"  "00"  ""  "feffffff"
        "02" // outputs
            "b8b4eb0b00000000"  "19"  "76a914a457b684d7f0d539a46a45bbc043f35b59d0d96388ac"
            "0008af2f00000000"  "19"  "76a914fd270b1ee6abcaea97fea7ad0402e8bd8ad6d77c88ac"
        "92040000" // nLockTime
    );
}

SigningInput buildInputP2SH_P2WPKH(bool omitScript = false, bool omitKeys = false, bool invalidOutputScript = false, bool invalidRedeemScript = false) {
    // Setup input
    SigningInput input;
    input.hashType = hashTypeForCoin(TWCoinTypeBitcoin);
    input.amount = 200'000'000;
    input.totalAmount = 200'000'000;
    input.byteFee = 1;
    input.toAddress = "1Bp9U1ogV3A14FMvKbRJms7ctyso4Z4Tcx";
    input.changeAddress = "1FQc5LdgGHMHEN9nwkjmz6tWkxhPpxBvBU";
    input.coinType = TWCoinTypeBitcoin;

    auto utxoKey0 = PrivateKey(parse_hex("eb696a065ef48a2192da5b28b694f87544b30fae8327c4510137a922f32c6dcf"));
    auto pubKey0 = utxoKey0.getPublicKey(TWPublicKeyTypeSECP256k1);
    auto utxoPubkeyHash = Hash::ripemd(Hash::sha256(pubKey0.bytes));
    assert(hex(utxoPubkeyHash) == "79091972186c449eb1ded22b78e40d009bdf0089");
    if (!omitKeys) {
        input.privateKeys.push_back(utxoKey0);
    }

    if (!omitScript && !invalidRedeemScript) {
        auto redeemScript = Script::buildPayToWitnessPublicKeyHash(utxoPubkeyHash);
        auto scriptHash = Hash::ripemd(Hash::sha256(redeemScript.bytes));
        assert(hex(scriptHash) == "4733f37cf4db86fbc2efed2500b4f4e49f312023");
        input.scripts[hex(scriptHash)] = redeemScript;
    } else if (invalidRedeemScript) {
        auto redeemScript = Script(parse_hex("FAFBFCFDFE"));
        auto scriptHash = Hash::ripemd(Hash::sha256(redeemScript.bytes));
        input.scripts[hex(scriptHash)] = redeemScript;
    }

    UTXO utxo0;
    auto utxo0Script = Script(parse_hex("a9144733f37cf4db86fbc2efed2500b4f4e49f31202387"));
    if (invalidOutputScript) {
        utxo0Script = Script(parse_hex("FFFEFDFCFB"));
    }
    utxo0.script = utxo0Script;
    utxo0.amount = 1'000'000'000;
    auto hash0 = parse_hex("db6b1b20aa0fd7b23880be2ecbd4a98130974cf4748fb66092ac4d3ceb1a5477");
    utxo0.outPoint = OutPoint(hash0, 1, UINT32_MAX);
    input.utxos.push_back(utxo0);

    return input;
}

TEST(BitcoinSigning, SignP2SH_P2WPKH) {
    auto input = buildInputP2SH_P2WPKH();
    {
        // test plan (but do not reuse plan result)
        auto plan = TransactionBuilder::plan(input);
        EXPECT_TRUE(verifyPlan(plan, {1'000'000'000}, 200'000'000, 170));
    }

    // Sign
    auto result = TransactionSigner<Transaction, TransactionBuilder>::sign(input);

    ASSERT_TRUE(result) << std::to_string(result.error());
    auto signedTx = result.payload();

    Data serialized;
    signedTx.encode(serialized);
    EXPECT_EQ(getEncodedTxSize(signedTx), (EncodedTxSize{251, 142, 170}));
    EXPECT_TRUE(validateEstimatedSize(signedTx, -1, 1));
    ASSERT_EQ(hex(serialized), // printed using prettyPrintTransaction
        "01000000" // version
        "0001" // marker & flag
        "01" // inputs
            "db6b1b20aa0fd7b23880be2ecbd4a98130974cf4748fb66092ac4d3ceb1a5477"  "01000000"  "17"  "16001479091972186c449eb1ded22b78e40d009bdf0089"  "ffffffff"
        "02" // outputs
            "00c2eb0b00000000"  "19"  "76a914769bdff96a02f9135a1d19b749db6a78fe07dc9088ac"
            "5607af2f00000000"  "19"  "76a9149e089b6889e032d46e3b915a3392edfd616fb1c488ac"
        // witness
            "02"  "47"  "3044022062b408cc7f92c8add622f3297b8992d68403849c6421ef58274ed6fc077102f30220250696eacc0aad022f55882d742dda7178bea780c03705bf9cdbee9f812f785301"  "21"  "03ad1d8e89212f0b92c74d23bb710c00662ad1470198ac48c43f7d6f93a2a26873"
        "00000000" // nLockTime
    );
}

TEST(BitcoinSigning, SignP2SH_P2WPKH_NegativeOmitScript) {
    auto input = buildInputP2SH_P2WPKH(true, false);
    {
        // test plan (but do not reuse plan result)
        auto plan = TransactionBuilder::plan(input);
        EXPECT_TRUE(verifyPlan(plan, {1'000'000'000}, 200'000'000, 174));
    }

    // Sign
    auto result = TransactionSigner<Transaction, TransactionBuilder>::sign(input);

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), Common::Proto::Error_script_redeem);
}

TEST(BitcoinSigning, SignP2SH_P2WPKH_NegativeInvalidOutputScript) {
    auto input = buildInputP2SH_P2WPKH(false, false, true);
    {
        // test plan (but do not reuse plan result)
        auto plan = TransactionBuilder::plan(input);
        EXPECT_TRUE(verifyPlan(plan, {1'000'000'000}, 200'000'000, 174));
    }

    // Sign
    auto result = TransactionSigner<Transaction, TransactionBuilder>::sign(input);

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), Common::Proto::Error_script_output);
}

TEST(BitcoinSigning, SignP2SH_P2WPKH_NegativeInvalidRedeemScript) {
    auto input = buildInputP2SH_P2WPKH(false, false, false, true);
    {
        // test plan (but do not reuse plan result)
        auto plan = TransactionBuilder::plan(input);
        EXPECT_TRUE(verifyPlan(plan, {1'000'000'000}, 200'000'000, 174));
    }

    // Sign
    auto result = TransactionSigner<Transaction, TransactionBuilder>::sign(input);

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), Common::Proto::Error_script_redeem);
}

TEST(BitcoinSigning, SignP2SH_P2WPKH_NegativeOmitKeys) {
    auto input = buildInputP2SH_P2WPKH(false, true);
    {
        // test plan (but do not reuse plan result). Plan works even with missing keys.
        auto plan = TransactionBuilder::plan(input);
        EXPECT_TRUE(verifyPlan(plan, {1'000'000'000}, 200'000'000, 170));
    }

    // Sign
    auto result = TransactionSigner<Transaction, TransactionBuilder>::sign(input);

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), Common::Proto::Error_missing_private_key);
}

TEST(BitcoinSigning, EncodeP2SH_P2WSH) {
    auto unsignedTx = Transaction(1);

    auto hash0 = parse_hex("36641869ca081e70f394c6948e8af409e18b619df2ed74aa106c1ca29787b96e");
    auto outpoint0 = OutPoint(hash0, 1);
    unsignedTx.inputs.emplace_back(outpoint0, Script(), 0xffffffff);

    auto outScript0 = Script(parse_hex("76a914389ffce9cd9ae88dcc0631e88a821ffdbe9bfe2688ac"));
    unsignedTx.outputs.emplace_back(0x0000000035a4e900, outScript0);

    auto outScript1 = Script(parse_hex("76a9147480a33f950689af511e6e84c138dbbd3c3ee41588ac"));
    unsignedTx.outputs.emplace_back(0x00000000052f83c0, outScript1);

    Data unsignedData;
    unsignedTx.encode(unsignedData, Transaction::SegwitFormatMode::NonSegwit);
    ASSERT_EQ(hex(unsignedData),
        "01000000" // version
        "01" // inputs
            "36641869ca081e70f394c6948e8af409e18b619df2ed74aa106c1ca29787b96e"  "01000000"  "00"  ""  "ffffffff"
        "02" // outputs
            "00e9a43500000000"  "19"  "76a914389ffce9cd9ae88dcc0631e88a821ffdbe9bfe2688ac"
            "c0832f0500000000"  "19"  "76a9147480a33f950689af511e6e84c138dbbd3c3ee41588ac"
        "00000000" // nLockTime
    );
}

TEST(BitcoinSigning, SignP2SH_P2WSH) {
    // Setup signing input
    SigningInput input;
    input.amount = 900000000;
    input.totalAmount = 900000000;
    input.hashType = (TWBitcoinSigHashType)0;
    input.toAddress = "16AQVuBMt818u2HBcbxztAZTT2VTDKupPS";
    input.changeAddress = "1Bd1VA2bnLjoBk4ook3H19tZWETk8s6Ym5";

    auto key0 = parse_hex("730fff80e1413068a05b57d6a58261f07551163369787f349438ea38ca80fac6");
    input.privateKeys.push_back(PrivateKey(key0));
    auto key1 = parse_hex("11fa3d25a17cbc22b29c44a484ba552b5a53149d106d3d853e22fdd05a2d8bb3");
    input.privateKeys.push_back(PrivateKey(key1));
    auto key2 = parse_hex("77bf4141a87d55bdd7f3cd0bdccf6e9e642935fec45f2f30047be7b799120661");
    input.privateKeys.push_back(PrivateKey(key2));
    auto key3 = parse_hex("14af36970f5025ea3e8b5542c0f8ebe7763e674838d08808896b63c3351ffe49");
    input.privateKeys.push_back(PrivateKey(key3));
    auto key4 = parse_hex("fe9a95c19eef81dde2b95c1284ef39be497d128e2aa46916fb02d552485e0323");
    input.privateKeys.push_back(PrivateKey(key4));
    auto key5 = parse_hex("428a7aee9f0c2af0cd19af3cf1c78149951ea528726989b2e83e4778d2c3f890");
    input.privateKeys.push_back(PrivateKey(key5));

    auto redeemScript = Script::buildPayToWitnessScriptHash(parse_hex("a16b5755f7f6f96dbd65f5f0d6ab9418b89af4b1f14a1bb8a09062c35f0dcb54"));
    auto scriptHash = Hash::ripemd(Hash::sha256(redeemScript.bytes));
    input.scripts[hex(scriptHash)] = redeemScript;

    auto witnessScript = Script(parse_hex(
        "56"
            "210307b8ae49ac90a048e9b53357a2354b3334e9c8bee813ecb98e99a7e07e8c3ba3"
            "2103b28f0c28bfab54554ae8c658ac5c3e0ce6e79ad336331f78c428dd43eea8449b"
            "21034b8113d703413d57761b8b9781957b8c0ac1dfe69f492580ca4195f50376ba4a"
            "21033400f6afecb833092a9a21cfdf1ed1376e58c5d1f47de74683123987e967a8f4"
            "2103a6d48b1131e94ba04d9737d61acdaa1322008af9602b3b14862c07a1789aac16"
            "2102d8b661b0b3302ee2f162b09e07a55ad5dfbe673a9f01d9f0c19617681024306b"
        "56ae"
    ));
    auto witnessScriptHash = Hash::ripemd(Hash::sha256(witnessScript.bytes));
    input.scripts[hex(witnessScriptHash)] = witnessScript;

    auto utxo0Script = Script(parse_hex("a9149993a429037b5d912407a71c252019287b8d27a587"));
    UTXO utxo;
    utxo.outPoint = OutPoint(parse_hex("36641869ca081e70f394c6948e8af409e18b619df2ed74aa106c1ca29787b96e"), 1, UINT32_MAX);
    utxo.script = utxo0Script;
    utxo.amount = 987654321;
    input.utxos.push_back(utxo);

    TransactionPlan plan;
    plan.amount = input.totalAmount;
    plan.availableAmount = input.utxos[0].amount;
    plan.change = 87000000;
    plan.fee = plan.availableAmount - plan.amount - plan.change;
    plan.utxos = input.utxos;
    input.plan = plan;

    // Sign
    auto result = TransactionSigner<Transaction, TransactionBuilder>::sign(input);

    ASSERT_TRUE(result) << std::to_string(result.error());
    auto signedTx = result.payload();

    auto expected =
        "01000000" // version
        "0001" // marker & flag
        "01" // inputs
            "36641869ca081e70f394c6948e8af409e18b619df2ed74aa106c1ca29787b96e"  "01000000"  "23"  "220020a16b5755f7f6f96dbd65f5f0d6ab9418b89af4b1f14a1bb8a09062c35f0dcb54"  "ffffffff"
        "02" // outputs
            "00e9a43500000000"  "19"  "76a914389ffce9cd9ae88dcc0631e88a821ffdbe9bfe2688ac"
            "c0832f0500000000"  "19"  "76a9147480a33f950689af511e6e84c138dbbd3c3ee41588ac"
        // witness
            "08"
                "00"  ""
                "47"  "304402201992f5426ae0bab04cf206d7640b7e00410297bfe5487637f6c2427ee8496be002204ad4e64dc2d269f593cc4820db1fc1e8dc34774f602945115ce485940e05c64200"
                "47"  "304402201e412363fa554b994528fd44149f3985b18bb901289ef6b71105b27c7d0e336c0220595e4a1e67154337757562ed5869127533e3e5084c3c2e128518f5f0b85b721800"
                "47"  "3044022003b0a20ccf545b3f12c5ade10db8717e97b44da2e800387adfd82c95caf529d902206aee3a2395530d52f476d0ddd9d20ba062820ae6f4e1be4921c3630395743ad900"
                "48"  "3045022100ed7a0eeaf72b84351bceac474b0c0510f67065b1b334f77e6843ed102f968afe022004d97d0cfc4bf5651e46487d6f87bd4af6aef894459f9778f2293b0b2c5b7bc700"
                "48"  "3045022100934a0c364820588154aed2d519cbcc61969d837b91960f4abbf0e374f03aa39d022036b5c58b754bd44cb5c7d34806c89d9778ea1a1c900618a841e9fbfbe805ff9b00"
                "47"  "3044022044e3b59b06931d46f857c82fa1d53d89b116a40a581527eac35c5eb5b7f0785302207d0f8b5d063ffc6749fb4e133db7916162b540c70dee40ec0b21e142d8843b3a00"
                "cf"  "56210307b8ae49ac90a048e9b53357a2354b3334e9c8bee813ecb98e99a7e07e8c3ba32103b28f0c28bfab54554ae8c658ac5c3e0ce6e79ad336331f78c428dd43eea8449b21034b8113d703413d57761b8b9781957b8c0ac1dfe69f492580ca4195f50376ba4a21033400f6afecb833092a9a21cfdf1ed1376e58c5d1f47de74683123987e967a8f42103a6d48b1131e94ba04d9737d61acdaa1322008af9602b3b14862c07a1789aac162102d8b661b0b3302ee2f162b09e07a55ad5dfbe673a9f01d9f0c19617681024306b56ae"
        "00000000" // nLockTime
        ;

    Data serialized;
    signedTx.encode(serialized);
    EXPECT_EQ(getEncodedTxSize(signedTx), (EncodedTxSize{800, 154, 316}));
    EXPECT_TRUE(validateEstimatedSize(signedTx, -1, 1));
    ASSERT_EQ(hex(serialized), expected);
}

TEST(BitcoinSigning, Sign_NegativeNoUtxos) {
    auto hash0 = parse_hex("fff7f7881a8099afa6940d42d1e7f6362bec38171ea3edf433541db4e4ad969f");
    auto hash1 = parse_hex("ef51e1b804cc89d182d279655c3aa89e815b1b309fe287d9b2b55d57b90ec68a");

    // Setup input
    SigningInput input;
    input.hashType = TWBitcoinSigHashTypeAll;
    input.amount = 335'790'000;
    input.totalAmount = 335'790'000;
    input.byteFee = 1;
    input.toAddress = "1Bp9U1ogV3A14FMvKbRJms7ctyso4Z4Tcx";
    input.changeAddress = "1FQc5LdgGHMHEN9nwkjmz6tWkxhPpxBvBU";

    auto scriptPub1 = Script(parse_hex("00141d0f172a0ecb48aee1be1f2687d2963ae33f71a1"));
    Data scriptHash;
    scriptPub1.matchPayToWitnessPublicKeyHash(scriptHash);
    auto scriptHashHex = hex(scriptHash);
    ASSERT_EQ(scriptHashHex, "1d0f172a0ecb48aee1be1f2687d2963ae33f71a1");

    auto redeemScript = Script::buildPayToPublicKeyHash(scriptHash);
    input.scripts[scriptHashHex] = redeemScript;

    {
        // plan returns empty, as there are 0 utxos
        auto plan = TransactionBuilder::plan(input);
        EXPECT_TRUE(verifyPlan(plan, {}, 0, 0, Common::Proto::Error_missing_input_utxos));
    }

    // Invoke Sign nonetheless
    auto result = TransactionSigner<Transaction, TransactionBuilder>::sign(input);

    // Fails as there are 0 utxos
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), Common::Proto::Error_missing_input_utxos);
}

TEST(BitcoinSigning, Sign_NegativeInvalidAddress) {
    auto hash0 = parse_hex("fff7f7881a8099afa6940d42d1e7f6362bec38171ea3edf433541db4e4ad969f");
    auto hash1 = parse_hex("ef51e1b804cc89d182d279655c3aa89e815b1b309fe287d9b2b55d57b90ec68a");

    // Setup input
    SigningInput input;
    input.hashType = TWBitcoinSigHashTypeAll;
    input.amount = 335'790'000;
    input.totalAmount = 335'790'000;
    input.byteFee = 1;
    input.toAddress = "THIS-IS-NOT-A-BITCOIN-ADDRESS";
    input.changeAddress = "THIS-IS-NOT-A-BITCOIN-ADDRESS-EITHER";

    auto utxoKey0 = PrivateKey(parse_hex("bbc27228ddcb9209d7fd6f36b02f7dfa6252af40bb2f1cbc7a557da8027ff866"));
    input.privateKeys.push_back(utxoKey0);

    auto utxoKey1 = PrivateKey(parse_hex("619c335025c7f4012e556c2a58b2506e30b8511b53ade95ea316fd8c3286feb9"));
    input.privateKeys.push_back(utxoKey1);

    auto scriptPub1 = Script(parse_hex("00141d0f172a0ecb48aee1be1f2687d2963ae33f71a1"));
    Data scriptHash;
    scriptPub1.matchPayToWitnessPublicKeyHash(scriptHash);
    auto scriptHashHex = hex(scriptHash);
    ASSERT_EQ(scriptHashHex, "1d0f172a0ecb48aee1be1f2687d2963ae33f71a1");

    auto redeemScript = Script::buildPayToPublicKeyHash(scriptHash);
    input.scripts[scriptHashHex] = redeemScript;

    UTXO utxo0;
    auto utxo0Script = Script(parse_hex("2103c9f4836b9a4f77fc0d81f7bcb01b7f1b35916864b9476c241ce9fc198bd25432ac"));
    utxo0.script = utxo0Script;
    utxo0.amount = 625'000'000;
    utxo0.outPoint = OutPoint(hash0, 0, UINT32_MAX);
    input.utxos.push_back(utxo0);

    UTXO utxo1;
    auto utxo1Script = Script(parse_hex("00141d0f172a0ecb48aee1be1f2687d2963ae33f71a1"));
    utxo1.script = utxo1Script;
    utxo1.amount = 600'000'000;
    utxo1.outPoint = OutPoint(hash1, 1, UINT32_MAX);
    input.utxos.push_back(utxo1);

    {
        // test plan (but do not reuse plan result)
        auto plan = TransactionBuilder::plan(std::move(input));
        EXPECT_TRUE(verifyPlan(plan, {625'000'000}, 335'790'000, 174));
    }

    // Sign
    auto result = TransactionSigner<Transaction, TransactionBuilder>::sign(input);

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), Common::Proto::Error_invalid_address);
}

TEST(BitcoinSigning, Plan_10input_MaxAmount) {
    auto ownAddress = "bc1q0yy3juscd3zfavw76g4h3eqdqzda7qyf58rj4m";
    auto ownPrivateKey = "eb696a065ef48a2192da5b28b694f87544b30fae8327c4510137a922f32c6dcf";

    SigningInput input;

    for (int i = 0; i < 10; ++i) {
        auto utxoScript = Script::lockScriptForAddress(ownAddress, TWCoinTypeBitcoin);
        Data keyHash;
        EXPECT_TRUE(utxoScript.matchPayToWitnessPublicKeyHash(keyHash));
        EXPECT_EQ(hex(keyHash), "79091972186c449eb1ded22b78e40d009bdf0089");

        auto redeemScript = Script::buildPayToPublicKeyHash(keyHash);
        input.scripts[std::string(keyHash.begin(), keyHash.end())] = redeemScript;

        UTXO utxo;
        utxo.script = utxoScript;
        utxo.amount = 1'000'000 + i * 10'000;
        auto hash = parse_hex("a85fd6a9a7f2f54cacb57e83dfd408e51c0a5fc82885e3fa06be8692962bc407");
        std::reverse(hash.begin(), hash.end());
        utxo.outPoint = OutPoint(hash, 0, UINT32_MAX);
        input.utxos.push_back(utxo);
    }

    input.coinType = TWCoinTypeBitcoin;
    input.hashType = hashTypeForCoin(TWCoinTypeBitcoin);
    input.useMaxAmount = true;
    input.amount = 2'000'000;
    input.totalAmount = 2'000'000;
    input.byteFee = 1;
    input.toAddress = "bc1qauwlpmzamwlf9tah6z4w0t8sunh6pnyyjgk0ne";
    input.changeAddress = ownAddress;

    // Plan.
    // Estimated size: witness size: 10 * (1 + 1 + 72 + 1 + 33) + 2 = 1082; base 451; raw 451 + 1082 = 1533; vsize 451 + 1082/4 --> 722
    // Actual size:    witness size:                                  1078; base 451; raw 451 + 1078 = 1529; vsize 451 + 1078/4 --> 721
    auto plan = TransactionBuilder::plan(input);
    EXPECT_TRUE(verifyPlan(plan, {1'000'000, 1'010'000, 1'020'000, 1'030'000, 1'040'000, 1'050'000, 1'060'000, 1'070'000, 1'080'000, 1'090'000}, 10'449'278, 722));

    // Extend input with keys, reuse plan, Sign
    auto privKey = PrivateKey(parse_hex(ownPrivateKey));
    input.privateKeys.push_back(privKey);
    input.plan = plan;

    // Sign
    auto result = TransactionSigner<Transaction, TransactionBuilder>::sign(input);

    ASSERT_TRUE(result) << std::to_string(result.error());
    auto signedTx = result.payload();

    Data serialized;
    signedTx.encode(serialized);
    EXPECT_EQ(getEncodedTxSize(signedTx), (EncodedTxSize{1529, 451, 721}));
    EXPECT_TRUE(validateEstimatedSize(signedTx, -1, 1));

    ASSERT_EQ(serialized.size(), 1529ul);
}

TEST(BitcoinSigning, Sign_LitecoinReal_a85f) {
    auto coin = TWCoinTypeLitecoin;
    auto ownAddress = "ltc1qt36tu30tgk35tyzsve6jjq3dnhu2rm8l8v5q00";
    auto ownPrivateKey = "b820f41f96c8b7442f3260acd23b3897e1450b8c7c6580136a3c2d3a14e34674";

    // Setup input
    SigningInput input;
    input.coinType = coin;
    input.hashType = hashTypeForCoin(coin);
    input.amount = 3'899'774;
    input.totalAmount = 3'899'774;
    input.useMaxAmount = true;
    input.byteFee = 1;
    input.toAddress = "ltc1q0dvup9kzplv6yulzgzzxkge8d35axkq4n45hum";
    input.changeAddress = ownAddress;

    auto privKey = PrivateKey(parse_hex(ownPrivateKey));
    input.privateKeys.push_back(privKey);

    auto utxo0Script = Script::lockScriptForAddress(ownAddress, coin);
    Data keyHash0;
    EXPECT_TRUE(utxo0Script.matchPayToWitnessPublicKeyHash(keyHash0));
    EXPECT_EQ(hex(keyHash0), "5c74be45eb45a3459050667529022d9df8a1ecff");

    auto redeemScript = Script::buildPayToPublicKeyHash(keyHash0);
    input.scripts[std::string(keyHash0.begin(), keyHash0.end())] = redeemScript;

    UTXO utxo0;
    utxo0.script = utxo0Script;
    utxo0.amount = 3'900'000;
    auto hash0 = parse_hex("7051cd18189401a844abf0f9c67e791315c4c154393870453f8ad98a818efdb5");
    std::reverse(hash0.begin(), hash0.end());
    utxo0.outPoint = OutPoint(hash0, 9, UINT32_MAX - 1);
    input.utxos.push_back(utxo0);

    // set plan, to match real tx
    TransactionPlan plan;
    plan.availableAmount = 3'900'000;
    plan.amount = 3'899'774;
    plan.fee = 226;
    plan.change = 0;
    plan.utxos.push_back(input.utxos[0]);
    input.plan = plan;
    EXPECT_TRUE(verifyPlan(input.plan.value(), {3'900'000}, 3'899'774, 226));

    // Sign
    auto result = TransactionSigner<Transaction, TransactionBuilder>::sign(input);

    ASSERT_TRUE(result) << std::to_string(result.error());
    auto signedTx = result.payload();

    Data serialized;
    signedTx.encode(serialized);

    // https://blockchair.com/litecoin/transaction/a85fd6a9a7f2f54cacb57e83dfd408e51c0a5fc82885e3fa06be8692962bc407
    ASSERT_EQ(hex(serialized), // printed using prettyPrintTransaction
        "01000000" // version
        "0001" // marker & flag
        "01" // inputs
            "b5fd8e818ad98a3f4570383954c1c41513797ec6f9f0ab44a801941818cd5170"  "09000000"  "00"  ""  "feffffff"
        "01" // outputs
            "7e813b0000000000"  "16"  "00147b59c096c20fd9a273e240846b23276c69d35815"
        // witness
            "02"
                "47"  "3044022029153096af176f9cca0ba9b827e947689a8bb8d11dda570c880f9108bc590b3002202410c78b666722ade1ef4547ad85a128ddcbd4695c40f942457bea3d043b9bb301"
                "21"  "036739829f2cfec79cfe6aaf1c22ecb7d4867dfd8ab4deb7121b36a00ab646caed"
        "00000000" // nLockTime
    );
}

TEST(BitcoinSigning, PlanAndSign_LitecoinReal_8435) {
    auto coin = TWCoinTypeLitecoin;
    auto ownAddress = "ltc1q0dvup9kzplv6yulzgzzxkge8d35axkq4n45hum";
    auto ownPrivateKey = "690b34763f34e0226ad2a4d47098269322e0402f847c97166e8f39959fcaff5a";

    // Setup input for Plan
    SigningInput input;
    input.coinType = coin;
    input.hashType = hashTypeForCoin(coin);
    input.amount = 1'200'000;
    input.totalAmount = 1'200'000;
    input.useMaxAmount = false;
    input.byteFee = 1;
    input.toAddress = "ltc1qt36tu30tgk35tyzsve6jjq3dnhu2rm8l8v5q00";
    input.changeAddress = ownAddress;

    auto utxo0Script = Script::lockScriptForAddress(ownAddress, coin);
    Data keyHash0;
    EXPECT_TRUE(utxo0Script.matchPayToWitnessPublicKeyHash(keyHash0));
    EXPECT_EQ(hex(keyHash0), "7b59c096c20fd9a273e240846b23276c69d35815");

    auto redeemScript = Script::buildPayToPublicKeyHash(keyHash0);
    input.scripts[std::string(keyHash0.begin(), keyHash0.end())] = redeemScript;

    UTXO utxo0;
    utxo0.script = utxo0Script;
    utxo0.amount = 3'899'774;
    auto hash0 = parse_hex("a85fd6a9a7f2f54cacb57e83dfd408e51c0a5fc82885e3fa06be8692962bc407");
    std::reverse(hash0.begin(), hash0.end());
    utxo0.outPoint = OutPoint(hash0, 0, UINT32_MAX);
    input.utxos.push_back(utxo0);

    // Plan
    auto plan = TransactionBuilder::plan(input);
    EXPECT_TRUE(verifyPlan(plan, {3'899'774}, 1'200'000, 141));

    // Extend input with keys and plan, for Sign
    auto privKey = PrivateKey(parse_hex(ownPrivateKey));
    input.privateKeys.push_back(privKey);
    input.plan = plan;

    // Sign
    auto result = TransactionSigner<Transaction, TransactionBuilder>::sign(input);

    ASSERT_TRUE(result) << std::to_string(result.error());
    auto signedTx = result.payload();

    Data serialized;
    signedTx.encode(serialized);
    EXPECT_EQ(getEncodedTxSize(signedTx), (EncodedTxSize{222, 113, 141}));
    EXPECT_TRUE(validateEstimatedSize(signedTx, -1, 1));

    // https://blockchair.com/litecoin/transaction/8435d205614ee70066060734adf03af4194d0c3bc66dd01bb124ab7fd25e2ef8
    ASSERT_EQ(hex(serialized), // printed using prettyPrintTransaction
        "01000000" // version
        "0001" // marker & flag
        "01" // inputs
            "07c42b969286be06fae38528c85f0a1ce508d4df837eb5ac4cf5f2a7a9d65fa8"  "00000000"  "00"  ""  "ffffffff"
        "02" // outputs
            "804f120000000000"  "16"  "00145c74be45eb45a3459050667529022d9df8a1ecff"
            "7131290000000000"  "16"  "00147b59c096c20fd9a273e240846b23276c69d35815"
        // witness
            "02"
                "47"  "304402204139b82927dd80445f27a5d2c29fa4881dbd2911714452a4a706145bc43cc4bf022016fbdf4b09bc5a9c43e79edb1c1061759779a20c35535082bdc469a61ed0771f01"
                "21"  "02499e327a05cc8bb4b3c34c8347ecfcb152517c9927c092fa273be5379fde3226"
        "00000000" // nLockTime
    );
}

TEST(BitcoinSigning, Sign_ManyUtxos_400) {
    auto ownAddress = "bc1q0yy3juscd3zfavw76g4h3eqdqzda7qyf58rj4m";
    auto ownPrivateKey = "eb696a065ef48a2192da5b28b694f87544b30fae8327c4510137a922f32c6dcf";

    // Setup input
    SigningInput input;

    const auto n = 400;
    uint64_t utxoSum = 0;
    for (int i = 0; i < n; ++i) {
        auto utxoScript = Script::lockScriptForAddress(ownAddress, TWCoinTypeBitcoin);
        Data keyHash;
        EXPECT_TRUE(utxoScript.matchPayToWitnessPublicKeyHash(keyHash));
        EXPECT_EQ(hex(keyHash), "79091972186c449eb1ded22b78e40d009bdf0089");

        auto redeemScript = Script::buildPayToPublicKeyHash(keyHash);
        input.scripts[std::string(keyHash.begin(), keyHash.end())] = redeemScript;

        UTXO utxo;
        utxo.script = utxoScript;
        utxo.amount = 1000 + (i + 1) * 10;
        auto hash = parse_hex("a85fd6a9a7f2f54cacb57e83dfd408e51c0a5fc82885e3fa06be8692962bc407");
        std::reverse(hash.begin(), hash.end());
        utxo.outPoint = OutPoint(hash, 0, UINT32_MAX);
        input.utxos.push_back(utxo);
        utxoSum += utxo.amount;
    }
    EXPECT_EQ(utxoSum, 1'202'000ul);

    input.coinType = TWCoinTypeBitcoin;
    input.hashType = hashTypeForCoin(TWCoinTypeBitcoin);
    input.useMaxAmount = false;
    input.amount = 300'000;
    input.totalAmount = 300'000;
    input.byteFee = 1;
    input.toAddress = "bc1qauwlpmzamwlf9tah6z4w0t8sunh6pnyyjgk0ne";
    input.changeAddress = ownAddress;

    // Plan
    auto plan = TransactionBuilder::plan(input);

    // expected result: 66 utxos, with the largest amounts
    std::vector<int64_t> subset;
    uint64_t subsetSum = 0;
    for (int i = n - 66; i < n; ++i) {
        const uint64_t val = 1000 + (i + 1) * 10;
        subset.push_back(val);
        subsetSum += val;
    }
    EXPECT_EQ(subset.size(), 66ul);
    EXPECT_EQ(subsetSum, 308'550ul);
    EXPECT_TRUE(verifyPlan(plan, subset, 300'000, 4'561));

    // Extend input with keys, reuse plan, Sign
    auto privKey = PrivateKey(parse_hex(ownPrivateKey));
    input.privateKeys.push_back(privKey);
    input.plan = plan;

    // Sign
    auto result = TransactionSigner<Transaction, TransactionBuilder>::sign(input);

    ASSERT_TRUE(result) << std::to_string(result.error());
    auto signedTx = result.payload();

    Data serialized;
    signedTx.encode(serialized);

    EXPECT_EQ(serialized.size(), 9871ul);
}

TEST(BitcoinSigning, Sign_ManyUtxos_2000) {
    auto ownAddress = "bc1q0yy3juscd3zfavw76g4h3eqdqzda7qyf58rj4m";
    auto ownPrivateKey = "eb696a065ef48a2192da5b28b694f87544b30fae8327c4510137a922f32c6dcf";

    // Setup input
    SigningInput input;

    const auto n = 2000;
    uint64_t utxoSum = 0;
    for (int i = 0; i < n; ++i) {
        auto utxoScript = Script::lockScriptForAddress(ownAddress, TWCoinTypeBitcoin);
        Data keyHash;
        EXPECT_TRUE(utxoScript.matchPayToWitnessPublicKeyHash(keyHash));
        EXPECT_EQ(hex(keyHash), "79091972186c449eb1ded22b78e40d009bdf0089");

        auto redeemScript = Script::buildPayToPublicKeyHash(keyHash);
        input.scripts[std::string(keyHash.begin(), keyHash.end())] = redeemScript;

        UTXO utxo;
        utxo.script = utxoScript;
        utxo.amount = 1000 + (i + 1) * 10;
        auto hash = parse_hex("a85fd6a9a7f2f54cacb57e83dfd408e51c0a5fc82885e3fa06be8692962bc407");
        std::reverse(hash.begin(), hash.end());
        utxo.outPoint = OutPoint(hash, 0, UINT32_MAX);
        input.utxos.push_back(utxo);
        utxoSum += utxo.amount;
    }
    EXPECT_EQ(utxoSum, 22'010'000ul);

    input.coinType = TWCoinTypeBitcoin;
    input.hashType = hashTypeForCoin(TWCoinTypeBitcoin);
    input.useMaxAmount = false;
    input.amount = 2'000'000;
    input.totalAmount = 2'000'000;
    input.byteFee = 1;
    input.toAddress = "bc1qauwlpmzamwlf9tah6z4w0t8sunh6pnyyjgk0ne";
    input.changeAddress = ownAddress;

    // Plan
    auto plan = TransactionBuilder::plan(input);

    // expected result: 601 utxos (smaller ones)
    std::vector<int64_t> subset;
    uint64_t subsetSum = 0;
    for (int i = 0; i < 601; ++i) {
        const uint64_t val = 1000 + (i + 1) * 10;
        subset.push_back(val);
        subsetSum += val;
    }
    EXPECT_EQ(subset.size(), 601ul);
    EXPECT_EQ(subsetSum, 2'410'010ul);
    EXPECT_TRUE(verifyPlan(plan, subset, 2'000'000, 40'943));

    // Extend input with keys, reuse plan, Sign
    auto privKey = PrivateKey(parse_hex(ownPrivateKey));
    input.privateKeys.push_back(privKey);
    input.plan = plan;

    // Sign
    auto result = TransactionSigner<Transaction, TransactionBuilder>::sign(input);

    ASSERT_TRUE(result) << std::to_string(result.error());
    auto signedTx = result.payload();

    Data serialized;
    signedTx.encode(serialized);

    EXPECT_EQ(serialized.size(), 89'339ul);
}

TEST(BitcoinSigning, EncodeThreeOutput) {
    auto coin = TWCoinTypeLitecoin;
    auto ownAddress = "ltc1qt36tu30tgk35tyzsve6jjq3dnhu2rm8l8v5q00";
    auto ownPrivateKey = "b820f41f96c8b7442f3260acd23b3897e1450b8c7c6580136a3c2d3a14e34674";
    auto toAddress0 = "ltc1qgknskahmm6svn42e33gum5wc4dz44wt9vc76q4";
    auto toAddress1 = "ltc1qulgtqdgxyd9nxnn5yxft6jykskz0ffl30nu32z";
    auto utxo0Amount = 3'851'829;
    auto toAmount0 = 1'000'000;
    auto toAmount1 = 2'000'000;

    auto unsignedTx = Transaction(1);

    auto hash0 = parse_hex("bbe736ada63c4678025dff0ff24d5f38970a3e4d7a2f77808689ed68004f55fe");
    std::reverse(hash0.begin(), hash0.end());
    auto outpoint0 = TW::Bitcoin::OutPoint(hash0, 0);
    unsignedTx.inputs.emplace_back(outpoint0, Script(), UINT32_MAX);

    auto lockingScript0 = Script::lockScriptForAddress(toAddress0, coin);
    unsignedTx.outputs.emplace_back(toAmount0, lockingScript0);
    auto lockingScript1 = Script::lockScriptForAddress(toAddress1, coin);
    unsignedTx.outputs.emplace_back(toAmount1, lockingScript1);
    // change
    auto lockingScript2 = Script::lockScriptForAddress(ownAddress, coin);
    unsignedTx.outputs.emplace_back(utxo0Amount - toAmount0 - toAmount1 - 172, lockingScript2);

    Data unsignedData;
    unsignedTx.encode(unsignedData, Transaction::SegwitFormatMode::Segwit);
    EXPECT_EQ(unsignedData.size(), 147ul);
    EXPECT_EQ(hex(unsignedData), // printed using prettyPrintTransaction
        "01000000" // version
        "0001" // marker & flag
        "01" // inputs
            "fe554f0068ed898680772f7a4d3e0a97385f4df20fff5d0278463ca6ad36e7bb"  "00000000"  "00"  ""  "ffffffff"
        "03" // outputs
            "40420f0000000000"  "16"  "001445a70b76fbdea0c9d5598c51cdd1d8ab455ab965"
            "80841e0000000000"  "16"  "0014e7d0b03506234b334e742192bd48968584f4a7f1"
            "c9fe0c0000000000"  "16"  "00145c74be45eb45a3459050667529022d9df8a1ecff"
        // witness
            "00"
        "00000000" // nLockTime
    );

    // add signature

    auto privkey = PrivateKey(parse_hex(ownPrivateKey));
    auto pubkey = PrivateKey(privkey).getPublicKey(TWPublicKeyTypeSECP256k1);
    EXPECT_EQ(hex(pubkey.bytes), "036739829f2cfec79cfe6aaf1c22ecb7d4867dfd8ab4deb7121b36a00ab646caed");

    auto utxo0Script = Script::lockScriptForAddress(ownAddress, coin); // buildPayToV0WitnessProgram()
    Data keyHashIn0;
    EXPECT_TRUE(utxo0Script.matchPayToWitnessPublicKeyHash(keyHashIn0));
    EXPECT_EQ(hex(keyHashIn0), "5c74be45eb45a3459050667529022d9df8a1ecff");

    auto redeemScript0 = Script::buildPayToPublicKeyHash(keyHashIn0);
    EXPECT_EQ(hex(redeemScript0.bytes), "76a9145c74be45eb45a3459050667529022d9df8a1ecff88ac");

    auto hashType = TWBitcoinSigHashType::TWBitcoinSigHashTypeAll;
    Data sighash = unsignedTx.getSignatureHash(redeemScript0, unsignedTx.inputs[0].previousOutput.index,
                                               hashType, utxo0Amount, static_cast<SignatureVersion>(unsignedTx._version));
    auto sig = privkey.signAsDER(sighash);
    ASSERT_FALSE(sig.empty());
    sig.push_back(hashType);
    EXPECT_EQ(hex(sig), "30450221008d88197a37ffcb51ecacc7e826aa588cb1068a107a82373c4b54ec42318a395c02204abbf5408504614d8f943d67e7873506c575e85a5e1bd92a02cd345e5192a82701");

    // add witness stack
    unsignedTx.inputs[0].scriptWitness.push_back(sig);
    unsignedTx.inputs[0].scriptWitness.push_back(pubkey.bytes);

    unsignedData.clear();
    unsignedTx.encode(unsignedData, Transaction::SegwitFormatMode::Segwit);
    EXPECT_EQ(unsignedData.size(), 254ul);
    // https://blockchair.com/litecoin/transaction/9e3fe98565a904d2da5ec1b3ba9d2b3376dfc074f43d113ce1caac01bf51b34c
    EXPECT_EQ(hex(unsignedData), // printed using prettyPrintTransaction
        "01000000" // version
        "0001" // marker & flag
        "01" // inputs
            "fe554f0068ed898680772f7a4d3e0a97385f4df20fff5d0278463ca6ad36e7bb"  "00000000"  "00"  ""  "ffffffff"
        "03" // outputs
            "40420f0000000000"  "16"  "001445a70b76fbdea0c9d5598c51cdd1d8ab455ab965"
            "80841e0000000000"  "16"  "0014e7d0b03506234b334e742192bd48968584f4a7f1"
            "c9fe0c0000000000"  "16"  "00145c74be45eb45a3459050667529022d9df8a1ecff"
        // witness
            "02"
                "48"  "30450221008d88197a37ffcb51ecacc7e826aa588cb1068a107a82373c4b54ec42318a395c02204abbf5408504614d8f943d67e7873506c575e85a5e1bd92a02cd345e5192a82701"
                "21"  "036739829f2cfec79cfe6aaf1c22ecb7d4867dfd8ab4deb7121b36a00ab646caed"
        "00000000" // nLockTime
    );
}

TEST(BitcoinSigning, RedeemExtendedPubkeyUTXO) {
    auto wif = "L4BeKzm3AHDUMkxLRVKTSVxkp6Hz9FcMQPh18YCKU1uioXfovzwP";
    auto decoded = Base58::decodeCheck(wif);
    auto key = PrivateKey(Data(decoded.begin() + 1, decoded.begin() + 33));
    auto pubkey = key.getPublicKey(TWPublicKeyTypeSECP256k1Extended);
    auto hash = Hash::sha256ripemd(pubkey.bytes.data(), pubkey.bytes.size());

    Data data;
    append(data, 0x00);
    append(data, hash);
    auto address = Bitcoin::Address(data);
    auto addressString = address.string();

    EXPECT_EQ(addressString, "1PAmpW5igXUJnuuzRa5yTcsWHwBamZG7Y2");

    // Setup input for Plan
    SigningInput input;
    input.coinType = TWCoinTypeBitcoin;
    input.hashType = hashTypeForCoin(TWCoinTypeBitcoin);
    input.amount = 26972;
    input.totalAmount = 26972;
    input.useMaxAmount = true;
    input.byteFee = 1;
    input.toAddress = addressString;

    auto utxo0Script = Script::lockScriptForAddress(addressString, TWCoinTypeBitcoin);

    UTXO utxo0;
    utxo0.script = utxo0Script;
    utxo0.amount = 16874;
    auto hash0 = parse_hex("6ae3f1d245521b0ea7627231d27d613d58c237d6bf97a1471341a3532e31906c");
    std::reverse(hash0.begin(), hash0.end());
    utxo0.outPoint = OutPoint(hash0, 0, UINT32_MAX);
    input.utxos.push_back(utxo0);

    UTXO utxo1;
    utxo1.script = utxo0Script;
    utxo1.amount = 10098;
    auto hash1 = parse_hex("fd1ea8178228e825d4106df0acb61a4fb14a8f04f30cd7c1f39c665c9427bf13");
    std::reverse(hash1.begin(), hash1.end());
    utxo1.outPoint = OutPoint(hash1, 0, UINT32_MAX);
    input.utxos.push_back(utxo1);

    input.privateKeys.push_back(key);

    // Sign
    auto result = TransactionSigner<Transaction, TransactionBuilder>::sign(input);

    ASSERT_TRUE(result) << std::to_string(result.error());
    auto signedTx = result.payload();

    Data encoded;
    signedTx.encode(encoded);
    EXPECT_EQ(encoded.size(), 402ul);
}

TEST(BitcoinSigning, SignP2TR_5df51e) {
    const auto privateKey = "13fcaabaf9e71ffaf915e242ec58a743d55f102cf836968e5bd4881135e0c52c";
    const auto ownAddress = "bc1qpjult34k9spjfym8hss2jrwjgf0xjf40ze0pp8";
    const auto toAddress = "bc1ptmsk7c2yut2xah4pgflpygh2s7fh0cpfkrza9cjj29awapv53mrslgd5cf"; // Taproot
    const auto coin = TWCoinTypeBitcoin;

    // Setup input
    SigningInput input;
    input.hashType = hashTypeForCoin(coin);
    input.amount = 1100;
    input.totalAmount = 1100;
    input.useMaxAmount = false;
    input.byteFee = 1;
    input.toAddress = toAddress;
    input.changeAddress = ownAddress;
    input.coinType = coin;

    auto utxoKey0 = PrivateKey(parse_hex(privateKey));
    auto pubKey0 = utxoKey0.getPublicKey(TWPublicKeyTypeSECP256k1);
    EXPECT_EQ(hex(pubKey0.bytes), "021e582a887bd94d648a9267143eb600449a8d59a0db0653740b1378067a6d0cee");
    EXPECT_EQ(SegwitAddress(pubKey0, "bc").string(), ownAddress);
    auto utxoPubkeyHash = Hash::ripemd(Hash::sha256(pubKey0.bytes));
    EXPECT_EQ(hex(utxoPubkeyHash), "0cb9f5c6b62c03249367bc20a90dd2425e6926af");
    input.privateKeys.push_back(utxoKey0);

    auto redeemScript = Script::lockScriptForAddress(input.toAddress, coin);
    EXPECT_EQ(hex(redeemScript.bytes), "51205ee16f6144e2d46edea1427e1222ea879377e029b0c5d2e252517aee85948ec7");
    auto scriptHash = Hash::ripemd(Hash::sha256(redeemScript.bytes));
    EXPECT_EQ(hex(scriptHash), "e0a5001e7b394a1a6b2978cdcab272241280bf46");
    input.scripts[hex(scriptHash)] = redeemScript;

    UTXO utxo0;
    auto utxo0Script = Script::lockScriptForAddress(ownAddress, coin);
    EXPECT_EQ(hex(utxo0Script.bytes), "00140cb9f5c6b62c03249367bc20a90dd2425e6926af");
    utxo0.script = utxo0Script;
    utxo0.amount = 49429;
    auto hash0 = parse_hex("c24bd72e3eaea797bd5c879480a0db90980297bc7085efda97df2bf7d31413fb");
    std::reverse(hash0.begin(), hash0.end());
    utxo0.outPoint = OutPoint(hash0, 1, UINT32_MAX);
    input.utxos.push_back(utxo0);

    {
        // test plan (but do not reuse plan result)
        auto plan = TransactionBuilder::plan(input);
        EXPECT_TRUE(verifyPlan(plan, {49429}, 1100, 153));
    }

    // Sign
    auto result = TransactionSigner<Transaction, TransactionBuilder>::sign(input);

    ASSERT_TRUE(result) << std::to_string(result.error());
    auto signedTx = result.payload();

    Data serialized;
    signedTx.encode(serialized);
    EXPECT_EQ(getEncodedTxSize(signedTx), (EncodedTxSize{234, 125, 153}));
    EXPECT_TRUE(validateEstimatedSize(signedTx, -1, 1));
    // https://mempool.space/tx/5df51e13bfeb79f386e1e17237f06d1b5c87c5bfcaa907c0c1cfe51cd7ca446d
    EXPECT_EQ(hex(serialized), // printed using prettyPrintTransaction
        "01000000" // version
        "0001" // marker & flag
        "01" // inputs
            "fb1314d3f72bdf97daef8570bc97029890dba08094875cbd97a7ae3e2ed74bc2"  "01000000"  "00"  ""  "ffffffff"
        "02" // outputs
            "4c04000000000000"  "22"  "51205ee16f6144e2d46edea1427e1222ea879377e029b0c5d2e252517aee85948ec7"
            "30bc000000000000"  "16"  "00140cb9f5c6b62c03249367bc20a90dd2425e6926af"
        // witness
            "02"
                "47"  "3044022021cea91157fdab33226e38ee7c1a686538fc323f5e28feb35775cf82ba8c62210220723743b150cea8ead877d8b8d059499779a5df69f9bdc755c9f968c56cfb528f01"
                "21"  "021e582a887bd94d648a9267143eb600449a8d59a0db0653740b1378067a6d0cee"
        "00000000" // nLockTime
    );
}

TEST(BitcoinSigning, Build_OpReturn_THORChainSwap_eb4c) {
    auto coin = TWCoinTypeBitcoin;
    auto ownAddress = "bc1q7s0a2l4aguksehx8hf93hs9yggl6njxds6m02g";
    auto toAddress = "bc1qxu5a8gtnjxw3xwdlmr2gl9d76h9fysu3zl656e";
    auto utxoAmount = 342101;
    auto toAmount = 300000;
    int fee = 36888;

    auto unsignedTx = Transaction(2, 0);

    auto hash0 = parse_hex("30b82960291a39de3664ec4c844a815e3e680e29b4d3a919e450f0c119cf4e35");
    std::reverse(hash0.begin(), hash0.end());
    auto outpoint0 = TW::Bitcoin::OutPoint(hash0, 1);
    unsignedTx.inputs.emplace_back(outpoint0, Script(), UINT32_MAX);

    auto lockingScriptTo = Script::lockScriptForAddress(toAddress, coin);
    unsignedTx.outputs.push_back(TransactionOutput(toAmount, lockingScriptTo));
    // change
    auto lockingScriptChange = Script::lockScriptForAddress(ownAddress, coin);
    unsignedTx.outputs.push_back(TransactionOutput(utxoAmount - toAmount - fee, lockingScriptChange));
    // memo OP_RETURN
    Data memo = data("SWAP:THOR.RUNE:thor1tpercamkkxec0q0jk6ltdnlqvsw29guap8wmcl:");
    auto lockingScriptOpReturn = Script::buildOpReturnScript(memo);
    EXPECT_EQ(hex(lockingScriptOpReturn.bytes), "6a3b535741503a54484f522e52554e453a74686f72317470657263616d6b6b7865633071306a6b366c74646e6c7176737732396775617038776d636c3a");
    unsignedTx.outputs.push_back(TransactionOutput(0, lockingScriptOpReturn));

    Data unsignedData;
    unsignedTx.encode(unsignedData, Transaction::SegwitFormatMode::Segwit);
    EXPECT_EQ(unsignedData.size(), 186ul);
    EXPECT_EQ(hex(unsignedData), // printed using prettyPrintTransaction
        "02000000" // version
        "0001" // marker & flag
        "01" // inputs
            "354ecf19c1f050e419a9d3b4290e683e5e814a844cec6436de391a296029b830"  "01000000"  "00"  ""  "ffffffff"
        "03" // outputs
            "e093040000000000"  "16"  "00143729d3a173919d1339bfd8d48f95bed5ca924391"
            "5d14000000000000"  "16"  "0014f41fd57ebd472d0cdcc7ba4b1bc0a4423fa9c8cd"
            "0000000000000000"  "3d"  "6a3b535741503a54484f522e52554e453a74686f72317470657263616d6b6b7865633071306a6b366c74646e6c7176737732396775617038776d636c3a"
        // witness
            "00"
        "00000000" // nLockTime
    );

    // add signature
    auto pubkey = parse_hex("0206121b83ebfddbb1997b50cb87b968190857269333e21e295142c8b88af9312a");
    auto sig = parse_hex("3045022100876eba8f9324d3fbb00b9dad9a34a8166dd75127d4facda63484c19703e9c178022052495a6229cc465d5f0fcf3cde3b22a0f861e762d0bb10acde26a57598bfe7e701");

    // add witness stack
    unsignedTx.inputs[0].scriptWitness.push_back(sig);
    unsignedTx.inputs[0].scriptWitness.push_back(pubkey);

    unsignedData.clear();
    unsignedTx.encode(unsignedData, Transaction::SegwitFormatMode::Segwit);
    EXPECT_EQ(unsignedData.size(), 293ul);
    // https://blockchair.com/bitcoin/transaction/eb4c1b064bfaf593d7cc6a5c73b75f932ffefe12a0478acf5a7e3145476683fc
    EXPECT_EQ(hex(unsignedData),
        "02000000000101354ecf19c1f050e419a9d3b4290e683e5e814a844cec6436de391a296029b8300100000000ffffffff03e0930400000000001600143729d3a1"
        "73919d1339bfd8d48f95bed5ca9243915d14000000000000160014f41fd57ebd472d0cdcc7ba4b1bc0a4423fa9c8cd00000000000000003d6a3b535741503a54"
        "484f522e52554e453a74686f72317470657263616d6b6b7865633071306a6b366c74646e6c7176737732396775617038776d636c3a02483045022100876eba8f"
        "9324d3fbb00b9dad9a34a8166dd75127d4facda63484c19703e9c178022052495a6229cc465d5f0fcf3cde3b22a0f861e762d0bb10acde26a57598bfe7e70121"
        "0206121b83ebfddbb1997b50cb87b968190857269333e21e295142c8b88af9312a00000000"
    );
}

TEST(BitcoinSigning, Sign_OpReturn_THORChainSwap) {
    PrivateKey privateKey = PrivateKey(parse_hex("6bd4096fa6f08bd3af2b437244ba0ca2d35045c5233b8d6796df37e61e974de5"));
    PublicKey publicKey = privateKey.getPublicKey(TWPublicKeyTypeSECP256k1);
    auto ownAddress = SegwitAddress(publicKey, "bc");
    auto ownAddressString = ownAddress.string();
    EXPECT_EQ(ownAddressString, "bc1q2gzg42w98ytatvmsgxfc8vrg6l24c25pydup9u");
    auto toAddress = "bc1qxu5a8gtnjxw3xwdlmr2gl9d76h9fysu3zl656e";
    auto utxoAmount = 342101;
    auto toAmount = 300000;
    int byteFee = 126;
    Data memo = data("SWAP:THOR.RUNE:thor1tpercamkkxec0q0jk6ltdnlqvsw29guap8wmcl:");

    SigningInput input;
    input.coinType = TWCoinTypeBitcoin;
    input.hashType = hashTypeForCoin(TWCoinTypeBitcoin);
    input.amount = toAmount;
    input.totalAmount = toAmount;
    input.byteFee = byteFee;
    input.toAddress = toAddress;
    input.changeAddress = ownAddressString;

    input.privateKeys.push_back(privateKey);
    input.outputOpReturn = memo;

    UTXO utxo;
    auto utxoHash = parse_hex("30b82960291a39de3664ec4c844a815e3e680e29b4d3a919e450f0c119cf4e35");
    std::reverse(utxoHash.begin(), utxoHash.end());
    utxo.outPoint = OutPoint(utxoHash, 1, UINT32_MAX);
    utxo.amount = utxoAmount;

    auto utxoPubkeyHash = Hash::ripemd(Hash::sha256(publicKey.bytes));
    EXPECT_EQ(hex(utxoPubkeyHash), "52048aa9c53917d5b370419383b068d7d55c2a81");
    auto utxoScript = Script::buildPayToWitnessPublicKeyHash(utxoPubkeyHash);
    EXPECT_EQ(hex(utxoScript.bytes), "001452048aa9c53917d5b370419383b068d7d55c2a81");
    utxo.script = utxoScript;
    input.utxos.push_back(utxo);

    {
        // test plan (but do not reuse plan result)
        auto plan = TransactionBuilder::plan(input);
        EXPECT_TRUE(verifyPlan(plan, {342101}, 300000, 26586));
        EXPECT_EQ(plan.outputOpReturn.size(), 59ul);
    }

    // Sign
    auto result = TransactionSigner<Transaction, TransactionBuilder>::sign(input);

    ASSERT_TRUE(result) << std::to_string(result.error());
    auto signedTx = result.payload();

    Data serialized;
    signedTx.encode(serialized);
    EXPECT_EQ(getEncodedTxSize(signedTx), (EncodedTxSize{293, 183, 211}));
    EXPECT_TRUE(validateEstimatedSize(signedTx, -1, 1));
    ASSERT_EQ(hex(serialized), // printed using prettyPrintTransaction
        "01000000" // version
        "0001" // marker & flag
        "01" // inputs
            "354ecf19c1f050e419a9d3b4290e683e5e814a844cec6436de391a296029b830"  "01000000"  "00"  ""  "ffffffff"
        "03" // outputs
            "e093040000000000"  "16"  "00143729d3a173919d1339bfd8d48f95bed5ca924391"
            "9b3c000000000000"  "16"  "001452048aa9c53917d5b370419383b068d7d55c2a81"
            "0000000000000000"  "3d"  "6a3b535741503a54484f522e52554e453a74686f72317470657263616d6b6b7865633071306a6b366c74646e6c7176737732396775617038776d636c3a"
        // witness
            "02"
                "48"  "3045022100ff6c0aaef512aa52f3036161bfbcef39046ac89eb9617fa461a0c9c43fe45eb3022055d208d3f81736e72e3ad8ef761dc79ac5dd3dc00721174bc69db416a74960e301"
                "21"  "02c2e5c8b4927812fb37444a7862466ad23978a4ac626f8eaf93e1d1a60d6abb80"
        "00000000" // nLockTime
    );
}
// clang-format on
} // namespace TW::Bitcoin

std::string nftInscriptionImageData = "\
89504e470d0a1a0a0000000d4948445200000360000002be0803000000f3\
0f8d7d000000d8504c54450000003070bf3070af3173bd3078b73870b730\
70b73575ba3075ba3075b53070ba3078bb3474bb3474b73074bb3074b733\
76b93076b93373bc3373b93073b93376bc3275ba3075ba3572ba3272ba33\
75bc3276b83474bb3274bb3274b83276bd3276bb3474bd3474bb3276b934\
74bb3474b93274bb3274b93476bb3276bb3375ba3275ba3373bc3273ba32\
77bc3375bc3375ba3373bc3374ba3174b93376bb3375bc3375bb3375b933\
75bb3374bb3376bb3475bb3375bb3375ba3374bb3276bc3276ba3375bc32\
75bc3375bb3275bb3374bc3374bb3375bb7edf10e10000004774524e5300\
10101f20202030303030404040404050505050505f606060606f70707070\
7f7f7f7f80808080808f8f909090909f9f9f9fa0a0afafafb0bfbfcfcfcf\
cfcfdfdfdfdfefefefef6a89059600001c294944415478daeddd6b7bd3d6\
ba2ee0d8350eb00c81ec5533cbac69d6da99383334dbe510904b66129383\
feff3fda17506888751892255b52eefb634b1259d2a3f1ead5d0f0d61600\
000000000000000000000000000000000000000000000000000000000000\
000000000000000000000000000000000000000000000000000000000000\
000000000000000000000000000000000000000000000000000000000000\
000000000000000000000000000000000084eaef3c9b4e274f87f604546e\
27ba8abf3a19db1b50a96114dfb01031a8d0cf57f18f0e7a760a54e4b778\
c9c2ad1854a27f1c27586cdb3350c1edd7224eb667dfc0aa9e5fc569666e\
c460b5f2f0df71063762b08afb8b389b3211ca978771aec82006a5fcf870\
39cd8587ce5066f8ba8ac3cc0c6250d0cec738d8c53fed2f2820bb79a89d\
08ab185fc545a91321b03a8ce21216ea44c8372c152f2fb1400d375fb722\
b6630f427abcf6aee2d544220675c54bc420adb5318b2be25e0c6ec72b8a\
2bb4186bdac3dfb5e1495cb5d903fb15bed48657711d4ec67d3b973b6eb8\
574fbafe1ac6343cb8cba5e1248a6bb69031eeead8557bba648c3b3a74ed\
1c9cc56b74f54e5b913b13aebde82a5ebf9399af8da0ebe17a7ab09170fd\
1db2b1ee3dddbce57a3a79b77a5978383e5fb95c8c0e9eba29a33ba3d683\
6793d94925e3d6f9686b6bf0a69ab12c3a98ec2819696baa86c39d67cfa6\
b377671556846fbe3e385e7d10fbdb59f4ee60fa6c6767e899340d8dd2b3\
835974d3c9d9592db759974fbefdc98a06b1a5b09d9dfdf041a2d94429c9\
66ed44ebea4f1cde1c63aa1cc4b29fa1ed2923d9dce8b5b6787dbefbbaa9\
a641ccbb2f3448eec2f1955587fbcbb748bbeb1ac4e203479a8de46b5d4f\
b4e683c4bfbf7fbda6bf3f73ac59bfe16233d5e106ea446318eb77bc9eea\
f0d7ac6d189cac27618f1c6ed66cbca99baf5b9bb1965bb185e3cd9a9dae\
bb35bfd18819c258af876be82d0c4207d3fa231639e2acd5a431f15a4bc4\
2e7cdd3a6bf57ec3f75eeb8e98b75ce8cc2d58f1787d89d8bcce80fde290\
b34ef5bdf13f2f3d37a9cee762024617027679385a65ab06b5558a0246fb\
03369facfe5ad6e88d802160950f5e3787b1530143c07e48d7eb51955b37\
383c1730da2caa345d35ac22bf5d6dc6048c7606ecf27054d77a18db93ea\
6ac5270e39ed0bd87c7f54ef660ec6c7972623721703f6e970773d4b398d\
267301e34e05ecf470cddfdd359acc2f058c3b10b04faf27a3cd2c42b8bd\
7b583e6502c65a9598ec7b393fdc54b66ea66c7a7c2a6034dddb2283d6e9\
ebe978bb496be76eef4e0ee7a702467b03f6e9d3fcf8f574321edd6bee87\
e88f46e3e9e1f1fcf4536ee9e875159a12b07fddbb77af7d1fa87fefde76\
c6f4949f1c721a12b0f64e7a1030044cc0b8038ed2cfc51702062b9aa69f\
8bd3d67ea86b0143c0ea93d14574c411300143c0040c5609d82b01831565\
7cf7c31f020602b66490b174b6238e80d51730df5f848009187721601f04\
0c56f43863259bb67ea68cef3cfbd31147c07c2604cc6782554ec63f050c\
3404040c015ba72e764611b03604ec0f479ca604ec42c060551d9c183b11\
3004ac3e9d7cc70d011330b8ad830bc40818cdd1c125ce8e040c01ab4f27\
175345c0040c6e3bedde57fdbc17301a23ea5ec03af891e862c09e0818a8\
a7040c1d81353af3059708d84602e6cb5558b30e3e95f5ed45344707e715\
59391b011330ee84eebd3cd513309aa37bafff5ad8977604ec8380c18a76\
3bb7c65917977aa4b51edfa580591691757bd8b9824ac070c7e2ae923b1f\
b08bce05ccaa6dac5bf79e1a4d058c06b94b01b3e60d6bd7b9a9b11695a2\
493af77247c60b382f1c6e1a14b076be9e68cd1b9aa4732fd877709511ba\
19b05f5c31a0be5b967606ec4cc0d013d848c0ac18c0da75aeab9df160af\
e770b36e19cf655f752d608e366bd7b5a97b0301a325016be5e4f3875e68\
a649baf64ab3179a69cb09b9e8d8e7f1be258dba676965c0bc6f89a6408d\
265e07a3513af6dcc8eb60344bc75e087b2b60344ac7de57f1b60acdd2b1\
af41f7b60aad39237f71bd80fa6e5ada389dde17c8d22c1d9b4e1f7b5b85\
46e9d63a82be1d8c86e9d674fa879d5ba998ee06ac8593f71e9b4c8f53b2\
3ebbe6fad22cddfafa8789b9beb4a72dd0bec988befa81a6e95463db5444\
9aa6538f66232bd3d330a75d9a2b652a224df3be4b177debfae2b6c50d25\
7748971a6f5645a471ba3495e3b1995234cd6e87a672ec5a1591a6e9d2fc\
d8899952b4e9bea56d53398e4ce4a071e2ee3c697e6f22078d73d69d87b3\
9135a568d359d9b627cdd79e33d3386fbbf31d7cb1256fd019a8cd43df1f\
4bf38c3bf3f0c873661a68b733a7a5c7603450771e841d593080e6e975a6\
35f0be3bed1a3ae4ac2b4f8f4ebdcf4cbbcecb76cd7f883d06a381de76a4\
4fffd063309a68da913efd63af5bd244e38ef4e927de06a36da5559b56b2\
38f2188c26caead3b7693e7da44b4f235d77a3bd7dad4b4f239d76a28dd8\
d3a5a799de76a23bf0d8a28834d3a4136dc489b9f434d36e27aefdef3511\
69a64127da88a7e6d2d350d75d6870c79a883454d4818bff634d449aeaa8\
03fd8189053968aa7107ba1c334d449a2a6b36625bdeb93cd544a4a9b226\
41bcf211604519ab0644edef71f87a66362c63b2d4453b3a0453af33d35c\
93d6f7b8233d0edad9e568c7c237b11e070d76ddf29bb0c7adbf42d06951\
cb6fc2a6e671d064472d3f4123f33868b2dd763f46ca7a0a6645299ca177\
fbfa40f79db57a3ae2cc6366da7b13d6fcd7a916195b3f7070d9bcac09f5\
8d6fd43f760b468b6fc21adf879bb905a3e94e5b5c232edc82d1e69bb086\
d7888f634fc168ba517bcfd299898834df756ba7f35d5b508ae6cb78272c\
fed8e40dcf6a807a178c569ca74d9e8f98350ff1dc71a521321bf50d6e73\
0c634d7ada206a679b63167b558536c85a37a0b96d8ecc014c85484b6ac4\
c6be76993980cd1c555a5223367408cb1cc0f4106992710b87b0990a91d6\
d488d7ad1bc2b207b0b1634a931c659eaec3b60d605e05a35932e72336f1\
59d8fdcc0df6c596b4a9cdd1c0373f162ddb5eeeb8cc4761f1a2697d8edf\
622d0ebad3e668da63a5ec0e871607cdf332fb9cfd6793b6b59f5d206a71\
d0c0212cfb9cbd68522731bb836816074df4266ecb6d58f60d98018c461a\
e59cb68de9d5ff1c1bc068a1a81d27eefd2b03185d1cc2e2bd56e4cb0046\
3befc21a91b0dc7c19c068aac175dcf4d161e72a3680d1562ff3cedef8dd\
667b893fe76ee0b9018cc6eae50e61f16293cfc37ecbddbcf8574791e69a\
e49fc18b8d4da4ed1fe76f9d5988345a947f0e6faad5717f11b06ddb0e21\
edee73c4717cb28932f179c086e970d0fe3ec7e73271edd3d5872143ab0e\
07dd2812e378b6de41ecf955d0568d1c3e3a5124c6f1628defaf840d5f71\
fc2f478fe69b849dcd6b6bd8f7f7023748079156f83df0848e0fd611b19d\
45e0d65cba01a3157a27a109abbfd9713f0add16eb04d0b1dbb02f11dba9\
b53afc77f086b801a33db6c3cfeb1afb89fdbdabf0cd78e3a8d11ee378f3\
117bb228b00dffe93b68b4c87e91842d6a983cb51315d9024f98e972c22a\
ef760c0bc54bbee87ac22a8d5891de867c713712164715dd8a15ea6dc817\
77266195743b0ac74bbeb833095b3d62e3452c5fdc1593c2095bada158ac\
75f8c55c7f9ef61a5d178f58e96ec7fde2f18a0f1d23da6c705efca42fb7\
6a47d1d6a1f951742261c725cefbe2b762c57b1b9fe7cf7bc192bbd8ea28\
1eb1e2bd8dcfd3a3b437e8c48d588932b1d01bcf257a1b6ebfe85099f8a6\
4c0042bb1dc35999dfae3ca44326d775d589a56ebe74e7e9da2056aa8a8b\
f7f296b2df59941abe2c904de706b1f3eaebc461b9d8ce75377027f6edfb\
5886155787e74f1c0b3a697c5eae4eacb23a8c0fdd7dd159a59e8925d689\
fde372d5e17d070175626e3ff179a9eaf0d2d26ca813f307b172cd8dcb7d\
d5212296ffc2f35ea951f058ef903b5227febe4ab363e763a99b2f333770\
2b96ff7511a5de4a89cfc58b3b16b179a941ac546fdec40ddc8ad5466f03\
11ab8f07cbdc5dfb7547ccb443743bea8b97de06225657bcb40ea1744351\
eb1036d6edd03a84fa2236132ff8a14edcd7db8016743bf43620d1fdf32a\
6ebeec47a8eb56ccbc0dc8aa137f77f3057546ecbd275fd0bc3a517508b5\
d589aa43088fd8b9ea106a54e8eb22ac67034507b1379e2cc3e69b1d9a1b\
50db2066f882fa0631c317d4368819bea0be41ccf005b50d62862fa844e2\
33b163c3175434889d9bba0135dab79e28ace94eecc4175542e5111b1f9f\
7efaf4e9f850730300000000000000000000000000000000000000000000\
000000000000000000000000000000000000000000000000000000000000\
00000000000000000068bff16c49effbfffc9fe5ff698f41016fe3253f7d\
ff9fd1f2ffb4c740c040c040c0046c15b3aa0ceedcae1b26ec8583b01f7d\
b2f483e3b01f7cbef483ff2b608d1657e5d19ddb75bdebd2bb61f9bcbd08\
fb9bcb7f7226600276778aab781a34f695dd7fbb35ec770113b0861a25ec\
8628e407c7093ff82aa8a05ffab173f7600276a76ac49f4a55888135620d\
15a28009585dfefbd992b5d488c3b23b30a1427cd2fa80f5968fc28e8075\
22602b9f3d256bc471e20e0ca81167657b234d0ed860f94ffc216002965a\
23f64a55884159a9a3421430016b6cc0126bc417e52ac4803d584b852860\
02d6dc808dcbd488e3943d987bf7564b85286002d6dc8095aa11a3943db8\
c8fb6b8b3a2a440113b0e6062c312c2fca5588b9bbf0f1f24f8c054cc03a\
1db01235e23875174e8b5688010d150113b036072ca946bce895aa107393\
b95c217ed8123001eb74c012e3f24bb90a316716484d15a2800958930356\
b8461c67ecc3e9062a4401abd728d5ff4938030ed3ff79ff6e06ac708d78\
9a11b0680315a2806dcaa068f17327039658233e2a5721660e4975558802\
d6ad800dd734d2f587c3dc3f55c9d933890b9d2093cc80bd2854210e56fb\
f8b5062cfcef0b5858c03e1fd21ffdb083fb3b93d9c9591cc7f3bf767fd6\
3fcefdd5c3f403fbec60767276f56d83cfa2e860b2d3af3160bdb8508d78\
9a19b0a84885384fd8530f6e7ffc77d3a70fd619b0efc7f9b3ab93e820ef\
cf0b5860c01e67eca6fe24baba795a14dca7d3c0226c78e3cfdc9a24f16e\
b25353c08ad588c39c5651af7c85387c363b4bfea557efc6c3b5042cf900\
2c664f05acce80ed1c5fddbaeed611b0fe5e9473f2465f4fb383e89b8493\
21baa5648df8aa5c85985123e654883b7b27d9bff8ddd3ba03d69fa41f80\
abd952c2bfefe2840d5fdc3a0a0702961eb061b45cd8541fb0fede55c8b3\
84689cf5a0372e35a2f58abc7a729ab781652ac47e5ebabe9eb5e33a0396\
7b00a25b254491a33017b0d4803dbf8aeb0f5860bcbe9c66412763a192b1\
408d38ccfd83bdc215e2f020f4c32f86b505ec79c036fc388a09581501eb\
1f27eeac8a03b6b388eb52b68ff8aa5c8598ba87532bc4e1acc8c7d9ab27\
603b1f8bff7901ab2060c38ff11a02f63c8e371ab00235e269ee1f8c8a55\
88e143f75727c31a02167e006e8ca102b67ac0fa8b780d01fb2dde70c0c2\
6bc4fc0a31a5c39f52210e3f16fe40cb65e2aa01eb17b9a95d6c0b587501\
3b8ed710b0c2f95a541eb049e8b4c2e57f781db68b8f922bc4de79bc7ac2\
560cd8b0607dfeb3805515b0ab780d012b9caf595479c0926ac4455085f8\
e751d88e48ab10c72506e545afca800d0bdffffe2c6015052c5e43c08685\
cfaf41f5010bad119737f6c528a8467c98da432c3184dd5e6760a5800d4b\
f4971e09587b02b6287e76d510b0c01a719250e75d8724f328f529739921\
ecd65254ab04ac4cbee28ba180b52560c5cfaf411d01eb85b503972bc4a4\
ecfc115221fef9ed7f9519c27e1c245709585426dff1494fc05a12b01203\
581d014bfc9d3fe55788e3a4c5812f422ac4ef53aa4665cef06945012bdb\
c03d10b076042cf96f5ebe9eec6edffb6c7bb43b3d3ebdf57375042ca946\
7c1152216e85d488e9156272043e7d9acf4f3f850e61e5039676037c7a38\
feb2ff479337e7a9b76102d682801d25fdfed1d2839addefc779be554fc0\
826ac4d3c4f3e6287f16484685786b08bb3c9eecdefbf67fb677d3ceef1f\
c25f3e6089f5c3e5e1f6cd6d1f9fa73c4e17b016042c6162c4bf927fc5e8\
cdf7dedbd1fc9b84b7fde7b784eeaca4d4f6022ac4901af1617640a2bf4f\
eda56b4bcaf9fd43f84b072c7102c77c10b409e3adbff77142486f1d05b3\
e9730336ff7665bd77afc28005e7ebf3dfdc3fbffd6d7515be0fff32bf46\
7c995ce7f5726bc4495685f83da1cb23f7d7b1f5f7c4e3d15b3d60891dc4\
5f93f6fc49e66342ef83ad1cb0cbfd7eeeaf2e13b0e54b7bf6f73dee3fa9\
2d6083fc1a719152f8bccdab114fb32ac4bf3e464abcbe7cea9c1ab16cc0\
923a1cff480ef971ea733c01ab2060fbfd805f5d2660cb7fb2d83a4b55ae\
e8925b233e4c3bc7f26ac48456c28f091cc5e7a3ac4d4b1ac3fe583d608b\
b0f1eb4bc24e32ae3e02b65ac0920e7e6d01fb636301cbad118fd2eabcbc\
1a31a142bcb5ccc5ef39cbcb649768250336ce9d2272f380677c46015b29\
60e783ad3506eccf8d052cb7465ca4b6c6a2ece754a7050be1840e4fe6e8\
5a326051d8a14e8de32b01ab22609783ad7506ecfb5cedb5072ce997dd7c\
dcf430fd2e649c35bee4578801aeb306c172011b66dd5685ec9e85805511\
b0f1568d014bdac27f6e2a602fb377d7517a2730bb46ccaf10f31d656d5a\
b9808d836629670d613f09d8ea017bbf5567c0121f562e0e1e6c24603935\
e222e3e169668db87a859878bff462c580bd2f7007967c15792160ab07ec\
51bd014b5d073075add1fa0296b4317f5fd41f668dede38c64565121263d\
aa9eae18b045ce1cfd25efd38eb880950fd89f5bf506ec28e3d1f655c692\
beb504ec65d605e628eb59712fe3141f075fb58a1dca57ab05ac97fdf03b\
c124ede410b0f2017b5173c0f2a7929f640d66d5066c94f5b8699139bd2e\
a3468c8a5588fd074f9f3d9bde7694b5bf4b052ce1684fb31da73ded13b0\
f2017b5073c002df863a3978daaf3f60893562488598d4c888d22bc4b47b\
9de1647612bec6d48a012bf5a267f25f11b0d201bbd8aa3b6093e0a3194d\
8675072ca3463cca2ea87aa9cfa9422bc49d8382ebb7ad18b06905017b20\
602b066c5e7bc00abd7b727bf1e6aa03965423be0aa91093b6e445810ab1\
3f39297c76af18b0a30a02f648c0560cd887fa033628f4cafcadaf20a8fa\
dbe5526bc48779cf07d36ac4a00af149998531560cd8db0a02f68b80ad18\
b069fd01dbda2e94b08b719d014bad118ff25a6e6935624085d83f2e7576\
0b98800505ace018f6c3f2e855072cb546ccab10536bc4fc0a7158725d7e\
0113b0b0806d0dde944d58e55f407c9d5c23e656886935627e8558365f4d\
08d813016b48c08e720aa571a141ec9ff505ec28b946ccad10d36ac4dc0a\
b174be9a10304d8ea604ec7dee9d48a1883daa2d604935e234a4424cda96\
5f022ac459bca18069d3b72d6059ef229f063c0c1abd09ce58545bc0926a\
c445488598d42089f22bc4151ef7ae18b0490501f3a0799d01cb7a553270\
16f1f6f8f8bcd010567dc0926ac407b390697bcbbbe4a2975b21a61488a7\
afa7d3f14d93ca03b6bbfcdfffefb8a02d015b63c02e0afde6d409af83db\
2b8d665dbdab0f58628d18522126d68851ce5e4a1cc0e6a37eb192bc54c0\
06e1134f4b9c6702b662c07a71d6410d6c1d64d8de9dbe9e5fa63f0dab2d\
604935e2554885985823e6558809ff2079f99bea0396f04123016b4cc092\
8abef40be0a270c0be8d6687f3ccdbeb1a0216348568103af8a575b6532f\
5329cb62d410b0f7a97b55c01a10b0b338fc02388e4b06eccb8748ec2ffe\
525bc04262320fde27d91562caf7caae27609390495c02b6a98025dd260d\
0b0c60455e3adc4fddb01a0296542306a6e065c05cca9c87d3e75b6b0bd8\
60b5832260f506ec6d1c7c01fc2d5e31600927c9abfa027654b2420c1afc\
9ee4fca90feb0b58d2ff59f4caecb15eb16736021612b0497068eec72b07\
6c9c7676bd2fd26aa9ac469c17a89b333badef8377f86e0d017b19172e12\
9f6f07de902f046cc580259e868b8422f1fe555ec0a271de07495d00f86d\
75bde60235e2b8f4e0f72170640e18c3570f58efba60c2faffefaf2f8f0d\
d8633d015b2d608947272161cf73a73b8de378b193fd4126691b7654a0d5\
525d8d38283df88df346b03fc2db442b072cf9839ea4dd496f0d3fa65c43\
9346eea980ad16b0b43791f77ef8473b51e07cc245e6287692364e8d736f\
736aa811e7e507bf5ede8d6cf2d3fac419c1ab076c90b88d2987a2bf7795\
5aa52454ea1743015b2d606973d916b3bf1681eaefec4501b39d1e7fffb9\
d4a730bfa5fef4c3a4fb9c9bf7093b653acfd7252bc4dcc1ef43fe3f4fba\
3cf43fc6b5046cebf7383462dfe295dc0839ca1b099f8f05ac68c07a1927\
d2d5d9d959de0dffb780cd12b2f9e3e53be985dfcc8dd81b7e8f57a99b81\
b7252bc4dcc16f9c5ff9250c10c38f714d01eb5da75e259fded88e0793e8\
e68df4492fa0057323a6fdff8e2a28dcef5cc08a7d53726ac06ecf373f39\
98ecfc7d6cfb3b9328732848de88ab683a9d1ebcbb2a7933302a5b21e60d\
7ebd8003b494b09d455c57c09293f17d2746b3d96c169de53742522eb657\
ef0ea6d3d9d7703e10b0a2011b5512b0e4f7a1cecea2283a39bbca1d0af2\
9fed7e2cf15ce7ba64859853237e08cbe3cd857dfacfa29a5e57f9ea4da9\
63372b7eb19d0a58d180ad38843d4a1cc0c29c0715aae56727bc2d320c85\
5f7596a399727d880e9eedececec3c3b88aef2df28582960bdf35207602f\
f0863ce85d0b01ab65087b94318015e876e7a7bcf8372d647eb20fe51b24\
83a02314aa9280155d71282561bdeb3aae73773d60694da802011b5e97f9\
d179a1945f146f73649d2fe3d2835fd2cddbf1a603b6b55dea10dcfe16b7\
fc523d12b0c2012b595fdc08d86fa50ac441b142b5c4dc8eb7252bc4ccc0\
27457370bde980954cd83f0a0f613d012b7c6bba7dbd62c0ca2ca97479bf\
e0295ae2da392a5b21669d6983228f1397ae2ad77505ac5495f88fe29f63\
2a60c5774ae86dd87f0e13035666c997cb27f90f9352cfb6d56bc471e9c1\
2fa5bd1fd8c71b9fd516b0ad41d185f12fff51e273440256e2aab31b3486\
cdfbd3c480bd2c9eaff3fb4bdbb05fc3b5f37de9426754309abda0b3fb70\
abc68005ecc2bc6310f2391e0958895333a4be384c5a85efcbee1e15ad4e\
e6497556dee951a2453c2e5921660c7e6913407a018d8e375bf506ac5099\
78dc2f77a578256065aefdb9eb5e5ffeba951ab0adadf1bc4869f26bf236\
4c2abf76a6c524604e5d547802c87e40be6a0ed8d6d6243062e74fca7695\
2f7a0256aab8ca5e94f7eb98334d3fe907a10b8e5eeef7cb8da39725a69a\
46a55b61e3e2d1ccd9fc2f9795ba03b635d83f5fe918e49e09ff190858c9\
bb97f41d3bff6b11b269e6a8b21b90b193cc439b756c67fd12fb6e5cb242\
4c1dfc06652f52b3af3f597bc002ca89f92467570ef62fd3a3a9c9b1427b\
20f1d05cbeffbec6df34af6c1b4d3396418ce7fba3fc4d382e7ec92d1a93\
71e9c16f9edb2e4adefcc36fc15c47c03eafe1355fe5187cfe05a7a5a2d9\
4abd84d58e0785fef9fdf03fd6df3dbc1991cbd3c39b0bd4de0fd990edcf\
8bfade8ad9e5e9ebc9a85f6a13e24fc79351e99db79bb45674d05dc420e9\
27ef17dffccb1f4ee9a5edf9fb2df0ffcadad0ff4a5df13ab50f3ab97d14\
3e1d1fee86076430fe7169e64faf3b99ae8de86f8fbe1cc2d1bd157ec7bd\
d1d75f32de1ddd2b7e64faa36f9bd0caa3fa7df3b7fb9b3e92bb7f6d48a9\
63f9d741dc5de54c00000000000000000000000000000000000000000000\
000000000000000000000000000000000000000000000000000000000000\
000000000000000000000000000000000000000000000000000000000000\
00000000000000000000000000000000000000000000008066fbffddd184\
8d4adc88950000000049454e44ae426082";

std::string nftInscriptionRawHex = "\
020000000001011771decbce2766b39d8fe66f4dc11737b3146c71f8cc6a\
e1397384c5e508e7f10000000000ffffffff012202000000000000160014\
e311b8d6ddff856ce8e9a4e03bc6d4fe5050a83d0340cc1e7b0b5fa18b28\
dce702e4e8ed2e91069d682b8daa3a773774bfc7d0e6f737d403016a9016\
b58a92592ad0b41682e6209167444eb56605532b28e9be922d3afdda1d00\
63036f7264010109696d6167652f706e67004d080289504e470d0a1a0a00\
00000d4948445200000360000002be0803000000f30f8d7d000000d8504c\
54450000003070bf3070af3173bd3078b73870b73070b73575ba3075ba30\
75b53070ba3078bb3474bb3474b73074bb3074b73376b93076b93373bc33\
73b93073b93376bc3275ba3075ba3572ba3272ba3375bc3276b83474bb32\
74bb3274b83276bd3276bb3474bd3474bb3276b93474bb3474b93274bb32\
74b93476bb3276bb3375ba3275ba3373bc3273ba3277bc3375bc3375ba33\
73bc3374ba3174b93376bb3375bc3375bb3375b93375bb3374bb3376bb34\
75bb3375bb3375ba3374bb3276bc3276ba3375bc3275bc3375bb3275bb33\
74bc3374bb3375bb7edf10e10000004774524e530010101f202020303030\
30404040404050505050505f606060606f707070707f7f7f7f8080808080\
8f8f909090909f9f9f9fa0a0afafafb0bfbfcfcfcfcfcfdfdfdfdfefefef\
ef6a89059600001c294944415478daeddd6b7bd3d6ba2ee0d8350eb00c81\
ec5533cbac69d6da99383334dbe510904b66129383feff3fda1750688875\
1892255b52eefb634b1259d2a3f1ead5d0f0d61600000000000000000000\
000000000000000000000000000000000000000000000000000000000000\
000000000000000000000000000000000000000000000000000000000000\
000000000000000000000000000000000000000000000000000000000000\
004d08020000000000000084eaef3c9b4e274f87f604546e27ba8abf3a19\
db1b50a96114dfb01031a8d0cf57f18f0e7a760a54e4b778c9c2ad1854a2\
7f1c27586cdb3350c1edd7224eb667dfc0aa9e5fc569666ec460b5f2f0df\
71063762b08afb8b389b3211ca978771aec82006a5fcf87039cd8587ce50\
66f8ba8ac3cc0c6250d0cec738d8c53fed2f2820bb79a89d08ab185fc545\
a91321b03a8ce21216ea44c8372c152f2fb1400d375fb722b6630f427abc\
f6aee2d544220675c54bc420adb5318b2be25e0c6ec72b8a2bb4186bdac3\
dfb5e1495cb5d903fb15bed48657711d4ec67d3b973b6eb8574fbafe1ac6\
343cb8cba5e1248a6bb69031eeead8557bba648c3b3a74ed1c9cc56b74f5\
4e5b913b13aebde82a5ebf9399af8da0ebe17a7ab09170fd1db2b1ee3ddd\
bce57a3a79b77a5978383e5fb95c8c0e9eba29a33ba3d6836793d94925e3\
d6f9686b6bf0a69ab12c3a98ec2819696baa86c39d67cfa6b37767155684\
6fbe3e385e7d10fbdb59f4ee60fa6c6767e899340d8dd2b3835974d3c9d9\
592db759974fbefdc98a06b1a5b09d9dfdf041a2d94429c966ed44ebea4f\
1cde1c63aa1cc4b29fa1ed2923d9dce8b5b6787dbefbbaa9a641ccbb2f34\
48eec2f1955587fbcbb748bbeb1ac4e203479a8de46b5d4fb4e683c4bfbf\
7fbda6bf3f73ac59bfe16233d5e106ea446318eb77bc9eeaf0d7ac6d189c\
ac27618f1c6ed66cbca99baf5b9b4d0802b1965bb185e3cd9a9daebb35bf\
d18819c258af876be82d0c4207d3fa231639e2acd5a431f15a4bc42e7cdd\
3a6bf57ec3f75eeb8e98b75ce8cc2d58f1787d89d8bcce80fde290b34ef5\
bdf13f2f3d37a9cee762024617027679385a65ab06b5558a0246fb03369f\
acfe5ad6e88d802160950f5e3787b1530143c07e48d7eb51955b37383c17\
30da2caa345d35ac22bf5d6dc6048c7606ecf27054d77a18db93ea6ac527\
0e39ed0bd87c7f54ef660ec6c7972623721703f6e970773d4b398d267301\
e34e05ecf470cddfdd359acc2f058c3b10b04faf27a3cd2c42b8bd7b583e\
6502c65a9598ec7b393fdc54b66ea66c7a7c2a6034dddb2283d6e9ebe978\
bb496be76eef4e0ee7a702467b03f6e9d3fcf8f574321edd6bee87e88f46\
e3e9e1f1fcf4536ee9e875159a12b07fddbb77af7d1fa87fefde76c6f494\
9f1c721a12b0f64e7a1030044cc0b8038ed2cfc51702062b9aa69f8bd3d6\
7ea86b0143c0ea93d14574c411300143c0040c5609d82b018315657cf7c3\
1f020602b66490b174b6238e80d51730df5f848009187721601f040c56f4\
3863259bb67ea68cef3cfbd31147c07c2604cc6782554ec63f050c340404\
0c015ba72e764611b03604ec0f479ca604ec42c060551d9c183b113004ac\
3e9d7cc70d011330b8ad830bc40818cdd1c125ce8e040c01ab4f27175345\
c0040c6e3bedde57fdbc17301a23ea5ec03af891e862c09e0818a84d0802\
a7040c1d81353af3059708d84602e6cb5558b30e3e95f5ed45344707e715\
59391b011330ee84eebd3cd513309aa37bafff5ad8977604ec8380c18a76\
3bb7c65917977aa4b51edfa580591691757bd8b9824ac070c7e2ae923b1f\
b08bce05ccaa6dac5bf79e1a4d058c06b94b01b3e60d6bd7b9a9b11695a2\
493af77247c60b382f1c6e1a14b076be9e68cd1b9aa4732fd877709511ba\
19b05f5c31a0be5b967606ec4cc0d013d848c0ac18c0da75aeab9df160af\
e770b36e19cf655f752d608e366bd7b5a97b0301a325016be5e4f3875e68\
a649baf64ab3179a69cb09b9e8d8e7f1be258dba676965c0bc6f89a6408d\
265e07a3513af6dcc8eb60344bc75e087b2b60344ac7de57f1b60acdd2b1\
af41f7b60aad39237f71bd80fa6e5ada389dde17c8d22c1d9b4e1f7b5b85\
46e9d63a82be1d8c86e9d674fa879d5ba998ee06ac8593f71e9b4c8f53b2\
3ebbe6fad22cddfafa8789b9beb4a72dd0bec988befa81a6e95463db5444\
9aa6538f66232bd3d330a75d9a2b652a224df3be4b177debfae2b6c50d25\
7748971a6f5645a471ba3495e3b1995234cd6e87a672ec5a1591a6e9d2fc\
d8899952b4e9bea56d53398e4ce4a071e2ee3c697e6f22078d73d69d87b3\
9135a568d359d9b627cdd79e33d3386fbbf31d7cb1256fd019a8cd43df1f\
4bf38c3bf3f0c873661a68b733a7a5c7603450771e841d593080e6e975a6\
35f0be3bed1a3ae4ac2b4d08024f8f4ebdcf4cbbcecb76cd7f883d06a381\
de76a44fffd063309a68da913efd63af5bd244e38ef4e927de06a36da555\
9b56b238f2188c26caead3b7693e7da44b4f235d77a3bd7dad4b4f239d76\
a28dd8d3a5a799de76a23bf0d8a28834d3a4136dc489b9f434d36e27aefd\
ef351169a64127da88a7e6d2d350d75d6870c79a883454d4818bff634d44\
9aeaa803fd8189053968aa7107ba1c334d449a2a6b36625bdeb93cd544a4\
a9b22641bcf211604519ab0644edef71f87a66362c63b2d4453b3a0453af\
33d35c93d6f7b8233d0edad9e568c7c237b11e070d76ddf29bb0c7adbf42\
d06951cb6fc2a6e671d064472d3f4123f33868b2dd763f46ca7a0a664529\
9ca177fbfa40f79db57a3ae2cc6366da7b13d6fcd7a916195b3f7070d9bc\
ac09f58d6fd43f760b468b6fc21adf879bb905a3e94e5b5c232edc82d1e6\
9bb086d7888f634fc168ba517bcfd299898834df756ba7f35d5b508ae6cb\
78272cfed8e40dcf6a807a178c569ca74d9e8f98350ff1dc71a521321bf5\
0d6e730c634d7ada206a679b63167b558536c85a37a0b96d8ecc014c8548\
4b6ac4c6be76993980cd1c555a5223367408cb1cc0f4106992710b87b099\
0a91d6d488d7ad1bc2b207b0b1634a931c659eaec3b60d605e05a35932e7\
2336f159d8fdcc0df6c596b4a9cdd1c0373f162ddb5eeeb8cc4761f1a269\
7d8edf622d0ebad3e668da63a5ec0e871607cdf332fb9c4d0802fd6793b6\
b59f5d206a71d0c0212cfb9cbd68522731bb836816074df4266ecb6d58f6\
0d98018c461ae59cb68de9d5ff1c1bc068a1a81d27eefd2b03185d1cc2e2\
bd56e4cb00463befc21a91b0dc7c19c068aac175dcf4d161e72a3680d156\
2ff3cedef8dd667b893fe76ee0b9018cc6eae50e61f16293cfc37ecbddbc\
f8574791e69ae49fc18b8d4da4ed1fe76f9d5988345a947f0e6faad5717f\
11b06ddb0e21edee73c4717cb28932f179c086e970d0fe3ec7e73271edd3\
d5872143ab0e07dd2812e378b6de41ecf955d0568d1c3e3a5124c6f1628d\
efaf840d5f71fc2f478fe69b849dcd6b6bd8f7f7023748079156f83df084\
8e0fd611b19d45e0d65cba01a3157a27a109abbfd9713f0add16eb04d0b1\
dbb02f11dba9b53afc77f086b801a33db6c3cfeb1afb89fdbdabf0cd78e3\
a8d11ee378f3117bb228b00dffe93b68b4c87e91842d6a983cb51315d902\
4f98e972c22aef760c0bc54bbee87ac22a8d5891de867c713712164715dd\
8a15ea6dc81777266195743b0ac74bbeb833095b3d62e3452c5fdc1593c2\
095bada158ac75f8c55c7f9ef61a5d178f58e96ec7fde2f18a0f1d23da6c\
705efca42fb76a47d1d6a1f951742261c725cefbe2b762c57b1b9fe7cf7b\
c192bbd8ea281eb1e2bd8dcfd3a3b437e8c48d588932b1d01bcf257a1b6e\
bfe85099f8a64c0042bb1dc35999dfae3ca44326d775d589a56ebe74e7e9\
da2056aa8a8b4d0802f7f296b2df59941abe2c904de706b1f3eaebc461b9\
d8ce75377027f6edfb5886155787e74f1c0b3a697c5eae4eacb23a8c0fdd\
7dd159a59e8925d689fde372d5e17d070175626e3ff179a9eaf0d2d26ca8\
13f307b172cd8dcb7dd5212296ffc2f35ea951f058ef903b5227febe4ab3\
63e763a99b2f3337702b96ff7511a5de4a89cfc58b3b16b179a941ac546f\
dec40ddc8ad5466f0311ab8f07cbdc5dfb7547ccb443743bea8b97de0622\
5657bcb40ea1744351eb1036d6edd03a84fa2236132ff8a14edcd7db8016\
743bf43620d1fdf32a6ebeec47a8eb56ccbc0dc8aa137f77f3057546ecbd\
275fd0bc3a517508b5d589aa43088fd8b9ea106a54e8eb22ac67034507b1\
379e2cc3e69b1d9a1b50db2066f882fa0631c317d4368819bea0be41ccf0\
05b50d62862fa844e233b163c3175434889d9bba0135dab79e28ace94eec\
c4175542e5111b1f9f7efaf4e9f850730300000000000000000000000000\
000000000000000000000000000000000000000000000000000000000000\
00000000000000000000000000000000000068bff16c49effbfffc9fe5ff\
698f41016fe3253f7dff9fd1f2ffb4c740c040c040c0046c15b3aa0ceedc\
ae1b26ec8583b01f7db2f483e3b01f7cbef483ff2b608d1657e5d19ddb75\
bdebd2bb61f9bcbd08fb9bcb7f7226600276778aab781a34f695dd7fbb35\
ec770113b0861a25ec8628e407c7093ff82aa84d0802a05ffab173f76002\
76a76ac49f4a55888135620d15a28009585dfefbd992b5d488c3b23b30a1\
427cd2fa80f5968fc28e807522602b9f3d256bc471e20e0ca81167657b23\
4d0ed860f94ffc216002965a23f64a55884159a9a3421430016b6cc0126b\
c417e52ac4803d584b85286002d6dc808dcbd488e3943d987bf7564b8528\
6002d6dc8095aa11a3943db8c8fb6b8b3a2a440113b0e6062c312c2fca55\
88b9bbf0f1f24f8c054cc03a1db01235e23875174e8b5688010d150113b0\
36072ca946bce895aa107393b95c217ed8123001eb74c012e3f24bb90a31\
6716484d15a2800958930356b8461c67ecc3e9062a4401abd728d5ff4938\
030ed3ff79ff6e06ac708d789a11b0680315a2806dcaa068f17327039658\
233e2a5721660e4975558802d6ad800dd734d2f587c3dc3f55c9d933890b\
9d2093cc80bd2854210e56fbf8b5062cfcef0b5858c03e1fd21ffdb083fb\
3b93d9c9591cc7f3bf767fd63fcefdd5c3f403fbec60767276f56d83cfa2\
e860b2d3af3160bdb8508d789a19b0a84885384fd8530f6e7ffc77d3a70f\
d619b0efc7f9b3ab93e820efcf0b5860c01e67eca6fe24baba795a14dca7\
d3c0226c78e3cfdc9a24f16eb25353c08ad588c39c5651af7c85387c363b\
4bfea557efc6c3b5042cf9002c664f05acce80ed1c5fddbaeed611b0fe5e\
9473f2465f4fb383e89b849321baa5648df8aa5c85985123e654883b7b27\
d9bf4d0802f8ddd3ba03d69fa41f80abd952c2bfefe2840d5fdc3a0a0702\
961eb061b45cd8541fb0fede55c8b384689cf5a0372e35a2f58abc7a729a\
b781652ac47e5ebabe9eb5e33a03967b00a25b254491a33017b0d4803dbf\
8aeb0f5860bcbe9c66412763a192b1408d38ccfd83bdc215e2f020f4c32f\
86b505ec79c036fc388a09581501eb1f27eeac8a03b6b388eb52b68ff8aa\
5c8598ba87532bc4e1acc8c7d9ab27603b1f8bff7901ab2060c38ff11a02\
f63c8e371ab00235e269ee1f8c8a5588e143f75727c31a02167e006e8ca1\
02b67ac0fa8b780d01fb2dde70c0c26bc4fc0a31a5c39f52210e3f16fe40\
cb65e2aa01eb17b9a95d6c0b5875013b8ed710b0c2f95a541eb049e8b4c2\
e57f781db68b8f922bc4de79bc7ac2560cd8b0607dfeb3805515b0ab780d\
012b9caf595479c0926ac4455085f8e751d88e48ab10c72506e545afca80\
0d0bdffffe2c6015052c5e43c08685cfaf41f5010bad119737f6c528a846\
7c98da432c3184dd5e6760a5800d4bf4971e09587b02b6287e76d510b0c0\
1a719250e75d8724f328f529739921ecd65254ab04ac4cbee28ba180b525\
60c5cfaf411d01eb85b503972bc4a4ecfc115221fef9ed7f9519c27e1c24\
5709585426dff1494fc05a12b01203581d014bfc9d3fe55788e3a4c5812f\
422ac4ef53aa4665cef06945012bdbc03d10b076042cf96f5ebe9eec6edf\
fb6c7bb43b3d3ebdf57375042ca9464d08027c1152216e85d488e9156272\
043e7d9acf4f3f850e61e5039676037c7a38feb2ff479337e7a9b76102d6\
82801d25fdfed1d2839addefc779be554fc0826ac4d3c4f3e6287f164846\
85786b08bb3c9eecdefbf67fb677d3ceef1fc25f3e6089f5c3e5e1f6cd6d\
1f9fa73c4e17b016042c6162c4bf927fc5e8cdf7dedbd1fc9b84b7fde7b7\
84eeaca4d4f6022ac4901af1617640a2bf4feda56b4bcaf9fd43f84b072c\
7102c77c10b409e3adbff77142486f1d05b3e9730336ff7665bd77afc280\
05e7ebf3dfdc3fbffd6d7515be0fff32bf467c995ce7f5726bc4495685f8\
3da1cb23f7d7b1f5f7c4e3d15b3d60891dc45f93f6fc49e66342ef83ad1c\
b0cbfd7eeeaf2e13b0e54b7bf6f73dee3fa92d6083fc1a719152f8bccdab\
114fb32ac4bf3e464abcbe7cea9c1ab16cc0923a1cff480ef971ea733c01\
ab2060fbfd805f5d2660cb7fb2d83a4b55aee8925b233e4c3bc7f26ac484\
56c28f091cc5e7a3ac4d4b1ac3fe583d608bb0f1eb4bc24e32ae3e02b65a\
c0920e7e6d01fb636301cbad118fd2eabcbc1a31a142bcb5ccc5ef39cbcb\
649768250336ce9d2272f380677c46015b2960e783ad3506eccf8d052cb7\
465ca4b6c6a2ece754a7050be1840e4fe6e85a326051d8a14e8de32b01ab\
22609783ad7506ecfb5cedb5072ce997dd7cdcf430fd2e649c35bee45788\
01aeb306c172011b66dd5685ec9e85805511b0f1568d014bdac27f6e4d08\
022a602fb377d7517a2730bb46ccaf10f31d656d5ab9808d836629670d61\
3f09d8ea017bbf5567c0121f562e0e1e6c24603935e222e3e169668db87a\
859878bff462c580bd2f7007967c15792160ab07ec51bd014b5d073075ad\
d1fa0296b4317f5fd41f668dede38c64565121263daa9eae18b045ce1cfd\
25efd38eb880950fd89f5bf506ec28e3d1f655c692beb504ec65d605e628\
eb59712fe3141f075fb58a1dca57ab05ac97fdf03bc124ede410b0f2017b\
5173c0f2a7929f640d66d5066c94f5b8699139bd2ea3468c8a5588fd074f\
9f3d9bde7694b5bf4b052ce1684fb31da73ded13b0f2017b5073c002df86\
3a3978daaf3f60893562488598d4c888d22bc4b47b9de1647612bec6d48a\
012bf5a267f25f11b0d201bbd8aa3b6093e0a3194d8675072ca3463cca2e\
a87aa9cfa9422bc49d8382ebb7ad18b06905017b20602b066c5e7bc00abd\
7b727bf1e6aa03965423be0aa91093b6e445810ab13f39297c76af18b0a3\
0a02f648c0560cd887fa033628f4cafcadaf20a8fadbe5526bc48779cf07\
d36ac4a00af149998531560cd8db0a02f68b80ad18b069fd01dbda2e94b0\
8b719d014bad118ff25a6e6935624085d83f2e75760b98800505ace018f6\
c3f2e855072cb546ccab10536bc4fc0a7158725d7e0113b0b0806d0dde94\
4d58e55f407c9d5c23e656886935627e8558365f4d08d813016b48c08e72\
0aa571a141ec9ff505ec284d0802b946ccad10d36ac4dc0ab174be9a1030\
4d8ea604ec7dee9d48a1883daa2d604935e234a4424cda965f022ac459bc\
a18069d3b72d6059ef229f063c0c1abd09ce58545bc0926ac445488598d4\
2089f22bc4151ef7ae18b0490501f3a0799d01cb7a55327016f1f6f8f8bc\
d010567dc0926ac407b390697bcbbbe4a2975b21a61488a7afa7d3f14d93\
ca03b6bbfcdfffefb8a02d015b63c02e0afde6d409af83db2b8d665dbdab\
0f58628d18522126d68851ce5e4a1cc0e6a37eb192bc54c006e1134f4b9c\
6702b662c07a71d6410d6c1d64d8de9dbe9e5fa63f0dab2d604935e25548\
85985823e6558809ff2079f99bea0396f04123016b4cc0928abef40be0a2\
70c0be8d6687f3ccdbeb1a0216348568103af8a575b6532f5329cb62d410\
b0f7a97b55c01a10b0b338fc02388e4b06eccb8748ec2ffe525bc0426232\
0fde27d91562caf7caae27609390495c02b6a98025dd260d0b0c60455e3a\
dc4fddb01a0296542306a6e065c05cca9c87d3e75b6b0bd860b5832260f5\
06ec6d1c7c01fc2d5e31600927c9abfa027654b2420c1afc9ee4fca90feb\
0b58d2ff59f4caecb15eb16736021612b0497068eec72b076c9c7676bd2f\
d26aa9ac469c17a89b333badef8377f86e0d017b19172e129f6f07de902f\
046cc580259e868b8422f1fe555ec0a271de07495d00f86d75bde60235e2\
b8f4e0f72170640e18c3570f58efba60c2faffefaf2f8f0d4d0802d8633d\
015b2d608947272161cf73a73b8de378b193fd4126691b7654a0d5525d8d\
38283df88df346b03fc2db442b072cf9839ea4dd496f0d3fa65c439346ee\
a980ad16b0b43791f77ef8473b51e07cc245e6287692364e8d736f736aa8\
11e7e507bf5ede8d6cf2d3fac419c1ab076c90b88d2987a2bf77955aa524\
54ea1743015b2d606973d916b3bf1681eaefec4501b39d1e7fffb9d4a730\
bfa5fef4c3a4fb9c9bf7093b653acfd7252bc4dcc1ef43fe3f4fba3cf43f\
c6b5046cebf7383462dfe295dc0839ca1b099f8f05ac68c07a1927d2d5d9\
d959de0dffb780cd12b2f9e3e53be985dfcc8dd81b7e8f57a99b81b7252b\
c4dcc16f9c5ff9250c10c38f714d01eb5da75e259fded88e0793e8e68df4\
492fa0057323a6fdff8e2a28dcef5cc08a7d53726ac06ecf373f3998ecfc\
7d6cfb3b9328732848de88ab683a9d1ebcbb2a7933302a5b21e60d7ebd80\
03b494b09d455c57c09293f17d2746b3d96c169de53742522eb657ef0ea6\
d3d9d7703e10b0a2011b5512b0e4f7a1cecea2283a39bbca1d0af29fed7e\
2cf15ce7ba64859853237e08cbe3cd857dfacfa29a5e57f9ea4da963372b\
7eb19d0a58d180ad38843d4a1cc0c29c0715aae56727bc2d320c855f7596\
a399727d880e9eedececec3c3b88aef2df28582960bdf35207602ff0863c\
e85d0b01ab65087b94318015e876e7a7bcf8372d647eb20fe51b2483a023\
14aa9280155d714d0802282561bdeb3aae73773d60694da802011b5e97f9\
d179a1945f146f73649d2fe3d2835fd2cddbf1a603b6b55dea10dcfe16b7\
fc523d12b0c2012b595fdc08d86fa50ac441b142b5c4dc8eb7252bc4ccc0\
27457370bde980954cd83f0a0f613d012b7c6bba7dbd62c0ca2ca97479bf\
e0295ae2da392a5b21669d6983228f1397ae2ad77505ac5495f88fe29f63\
2a60c5774ae86dd87f0e13035666c997cb27f90f9352cfb6d56bc471e9c1\
2fa5bd1fd8c71b9fd516b0ad41d185f12fff51e273440256e2aab31b3486\
cdfbd3c480bd2c9eaff3fb4bdbb05fc3b5f37de9426754309abda0b3fb70\
abc68005ecc2bc6310f2391e0958895333a4be384c5a85efcbee1e15ad4e\
e6497556dee951a2453c2e5921660c7e6913407a018d8e375bf506ac5099\
78dc2f77a578256065aefdb9eb5e5ffeba951ab0adadf1bc4869f26bf236\
4c2abf76a6c524604e5d547802c87e40be6a0ed8d6d6243062e74fca7695\
2f7a0256aab8ca5e94f7eb98334d3fe907a10b8e5eeef7cb8da39725a69a\
46a55b61e3e2d1ccd9fc2f9795ba03b635d83f5fe918e49e09ff190858c9\
bb97f41d3bff6b11b269e6a8b21b90b193cc439b756c67fd12fb6e5cb242\
4c1dfc06652f52b3af3f597bc002ca89f92467570ef62fd3a3a9c9b1427b\
20f1d05cbeffbec6df34af6c1b4d3396418ce7fba3fc4d382e7ec92d1a93\
71e9c16f9edb2e4adefcc36fc15c47c03eafe1354d29015fe5187cfe05a7\
a5a2d94abd84d58e0785fef9fdf03fd6df3dbc1991cbd3c39b0bd4de0fd9\
90edcf8bfade8ad9e5e9ebc9a85f6a13e24fc79351e99db79bb45674d05d\
c420e927ef17dffccb1f4ee9a5edf9fb2df0ffcadad0ff4a5df13ab50f3a\
b97d143e1d1fee86076430fe7169e64faf3b99ae8de86f8fbe1cc2d1bd15\
7ec7bdd1d75f32de1ddd2b7e64faa36f9bd0caa3fa7df3b7fb9b3e92bb7f\
6d48a963f9d741dc5de54c00000000000000000000000000000000000000\
000000000000000000000000000000000000000000000000000000000000\
000000000000000000000000000000000000000000000000000000000000\
00000000000000000000000000000000000000000000000000008066fbff\
ddd1848d4adc88950000000049454e44ae4260826821c00f209b6ada5edb\
42c77fd2bc64ad650ae38314c8f451f3e36d80bc8e26f132cb00000000";