package libdogecoin

import (
	"strings"
	"testing"
)

// internal keys (set 1)
var privkey_wif string = "ci5prbqz7jXyFPVWKkHhPq4a9N8Dag3TpeRfuqqC2Nfr7gSqx1fy"
var p2pkh_addr string = "noxKJyGPugPRN4wqvrwsrtYXuQCk7yQEsy"
var utxo_scriptpubkey string = "76a914d8c43e6f68ca4ea1e9b93da2d1e3a95118fa4a7c88ac"

// internal keys (set 2)
var privkey_wif2 string = "cf6dS36Erx417gQgYiWXyXwQL5u4yxkEhUVuD65u3EKSbT3Bfaor"
var p2pkh_addr2 string = "ncMfpXNvKzeNBoJwuAdSjz949oWLyieWee"
var utxo_scriptpubkey2 string = "76a914598d6c0f67763fa68235edcd60ab939fc242875c88ac"

// external keys
var external_p2pkh_addr string = "nbGfXLskPh7eM1iG5zz5EfDkkNTo9TRmde"

// expected hashes step by step for integer test amount (using set 1)
var expected_empty_tx_hex string = "01000000000000000000"
var expected_unsigned_single_utxo_tx_hex string = "0100000001746007aed61e8531faba1af6610f10a5422c70a2a7eb6ffb51cb7a7b7b5e45b40100000000ffffffff0000000000"
var expected_unsigned_double_utxo_tx_hex string = "0100000002746007aed61e8531faba1af6610f10a5422c70a2a7eb6ffb51cb7a7b7b5e45b40100000000ffffffffe216461c60c629333ac6b40d29b5b0b6d0ce241aea5903cf4329fc65dc3b11420100000000ffffffff0000000000"
var expected_unsigned_double_utxo_single_output_tx_hex string = "0100000002746007aed61e8531faba1af6610f10a5422c70a2a7eb6ffb51cb7a7b7b5e45b40100000000ffffffffe216461c60c629333ac6b40d29b5b0b6d0ce241aea5903cf4329fc65dc3b11420100000000ffffffff010065cd1d000000001976a9144da2f8202789567d402f7f717c01d98837e4325488ac00000000"
var expected_unsigned_tx_hex string = "0100000002746007aed61e8531faba1af6610f10a5422c70a2a7eb6ffb51cb7a7b7b5e45b40100000000ffffffffe216461c60c629333ac6b40d29b5b0b6d0ce241aea5903cf4329fc65dc3b11420100000000ffffffff020065cd1d000000001976a9144da2f8202789567d402f7f717c01d98837e4325488ac30b4b529000000001976a914d8c43e6f68ca4ea1e9b93da2d1e3a95118fa4a7c88ac00000000"
var expected_signed_single_input_tx_hex string = "0100000002746007aed61e8531faba1af6610f10a5422c70a2a7eb6ffb51cb7a7b7b5e45b4010000006b48304502210090bddac300243d16dca5e38ab6c80d5848e0d710d77702223bacd6682654f6fe02201b5c2e8b1143d8a807d604dc18068b4278facce561c302b0c66a4f2a5a4aa66f0121031dc1e49cfa6ae15edd6fa871a91b1f768e6f6cab06bf7a87ac0d8beb9229075bffffffffe216461c60c629333ac6b40d29b5b0b6d0ce241aea5903cf4329fc65dc3b11420100000000ffffffff020065cd1d000000001976a9144da2f8202789567d402f7f717c01d98837e4325488ac30b4b529000000001976a914d8c43e6f68ca4ea1e9b93da2d1e3a95118fa4a7c88ac00000000"
var expected_signed_raw_tx_hex string = "0100000002746007aed61e8531faba1af6610f10a5422c70a2a7eb6ffb51cb7a7b7b5e45b4010000006b48304502210090bddac300243d16dca5e38ab6c80d5848e0d710d77702223bacd6682654f6fe02201b5c2e8b1143d8a807d604dc18068b4278facce561c302b0c66a4f2a5a4aa66f0121031dc1e49cfa6ae15edd6fa871a91b1f768e6f6cab06bf7a87ac0d8beb9229075bffffffffe216461c60c629333ac6b40d29b5b0b6d0ce241aea5903cf4329fc65dc3b1142010000006a47304402200e19c2a66846109aaae4d29376040fc4f7af1a519156fe8da543dc6f03bb50a102203a27495aba9eead2f154e44c25b52ccbbedef084f0caf1deedaca87efd77e4e70121031dc1e49cfa6ae15edd6fa871a91b1f768e6f6cab06bf7a87ac0d8beb9229075bffffffff020065cd1d000000001976a9144da2f8202789567d402f7f717c01d98837e4325488ac30b4b529000000001976a914d8c43e6f68ca4ea1e9b93da2d1e3a95118fa4a7c88ac00000000"

// expected hashes step by step for decimal test amount (using set 2)
var expected_unsigned_single_utxo_tx_hex2 string = "01000000018746fb1cc227513513063245373c2d16533ca638b6911e34b01b7970681f099b0100000000ffffffff0000000000"
var expected_unsigned_single_utxo_single_output_tx_hex2 string = "01000000018746fb1cc227513513063245373c2d16533ca638b6911e34b01b7970681f099b0100000000ffffffff01cc74e0c7020000001976a9144da2f8202789567d402f7f717c01d98837e4325488ac00000000"
var expected_unsigned_tx_hex2 string = "01000000018746fb1cc227513513063245373c2d16533ca638b6911e34b01b7970681f099b0100000000ffffffff01cc74e0c7020000001976a9144da2f8202789567d402f7f717c01d98837e4325488ac00000000"
var expected_signed_raw_tx_hex2 string = "01000000018746fb1cc227513513063245373c2d16533ca638b6911e34b01b7970681f099b010000006b483045022100f78f4b911b74c8769d3c6824d048c7d6813265d8599dfcc19bc12e17fcf0207b02206ae0a48e4319767cce4579f4852e179ba9cd6061f1c337eac782facdbff42a49012102eab8cb0125caff77443d47d1d6bbc8f753fe17b86c2ea3332622b3db82afe6f4ffffffff01cc74e0c7020000001976a9144da2f8202789567d402f7f717c01d98837e4325488ac00000000"

// existing transactions
var hash_2_doge string = "b4455e7b7b7acb51fb6feba7a2702c42a5100f61f61abafa31851ed6ae076074"       // 2 DOGE
var hash_10_doge string = "42113bdc65fc2943cf0359ea1a24ced0b6b0b5290db4c63a3329c6601c4616e2"      // 10 DOGE
var hash_decimal_doge string = "9b091f6870791bb0341e91b638a63c53162d3c3745320613355127c21cfb4687" // 119.43536540 DOGE
var vout_2_doge int = 1                                                                           // vout is the spendable output index from the existing transaction
var vout_10_doge int = 1
var vout_decimal_doge int = 1

// transaction amounts
var input1_amt string = "2.0"
var input2_amt string = "10.0"
var send_amt string = "5.0"
var total_utxo_input string = "12.0"

var decimal_input_amt string = "119.43536540"
var decimal_send_amt string = "119.43310540" // fee of 0.00226 deducted
var decimal_total_utxo_input string = "119.43536540"

var fee string = "0.00226"

// invalid parameters
var bad_privkey_wif string = "ci5prbqz7jXyFPVWKkHhPq4a9N8Dag3TpeRfuqqC2Nfr7gSqx1fx"
var long_tx_hex string = strings.Repeat("x", (1024*100)+1)
var high_send_amt int = 15 // max we can spend from the two inputs is 12

func TestTransaction(t *testing.T) {
	t.Run("start_transaction", func(t *testing.T) {
		res := W_start_transaction()
		if res <= 0 {
			t.Errorf("Invalid index received.")
		}
		res2 := W_start_transaction()
		if res2 != res+1 {
			t.Errorf("Transactions are not consecutive.")
		}
		if W_get_raw_transaction(res) != expected_empty_tx_hex || W_get_raw_transaction(res2) != expected_empty_tx_hex {
			t.Errorf("Transaction state does not match expected hash.")
		}
		W_clear_transaction(res)
		W_clear_transaction(res2)
	})

	t.Run("store_raw_transaction", func(t *testing.T) {
		idx := W_start_transaction()
		idx2 := W_store_raw_transaction(expected_unsigned_single_utxo_tx_hex)
		if idx2 != idx+1 {
			t.Errorf("Transactions are not consecutive.")
		}
		if W_get_raw_transaction(idx2) != expected_unsigned_single_utxo_tx_hex {
			t.Errorf("Transaction hex not stored properly.")
		}
		W_clear_transaction(idx)
		W_clear_transaction(idx2)
	})

	t.Run("store_long_raw_transaction", func(t *testing.T) {
		res := W_store_raw_transaction(long_tx_hex)
		if res != 0 {
			t.Errorf("Long transaction hex should not be saved.")
		}
	})

	t.Run("get_raw_transaction", func(t *testing.T) {
		idx := W_start_transaction()
		W_add_utxo(idx, hash_2_doge, vout_2_doge)
		if W_get_raw_transaction(idx) != expected_unsigned_single_utxo_tx_hex {
			t.Errorf("Returned hex does not match expected hex after adding the first utxo.")
		}
		W_add_utxo(idx, hash_10_doge, vout_10_doge)
		if W_get_raw_transaction(idx) != expected_unsigned_double_utxo_tx_hex {
			t.Errorf("Returned hex does not match expected hex after adding the second utxo.")
		}
		W_add_output(idx, external_p2pkh_addr, send_amt)
		if W_get_raw_transaction(idx) != expected_unsigned_double_utxo_single_output_tx_hex {
			t.Errorf("Returned hex does not match expected hex after adding both utxos and an output.")
		}
		W_clear_transaction(idx)
	})

	t.Run("get_raw_transaction_bad_index", func(t *testing.T) {
		idx := W_start_transaction()
		res := W_get_raw_transaction(idx + 1) // out of bounds
		if res != "" {
			t.Errorf("Bad index should return empty string.")
		}
		W_clear_transaction(idx)
	})

	t.Run("single_utxo", func(t *testing.T) {
		idx := W_start_transaction()
		if W_add_utxo(idx, hash_2_doge, vout_2_doge) == 0 {
			t.Errorf("Error while running add_utxo().")
		}
		rawhex := W_get_raw_transaction(idx)
		if rawhex != expected_unsigned_single_utxo_tx_hex {
			t.Errorf("Transaction state does not match expected hash.")
		}
		W_clear_transaction(idx)
	})

	t.Run("double_utxo", func(t *testing.T) {
		idx := W_start_transaction()
		if W_add_utxo(idx, hash_2_doge, vout_2_doge) == 0 || W_add_utxo(idx, hash_10_doge, vout_10_doge) == 0 {
			t.Errorf("Error while running add_utxo().")
		}
		rawhex := W_get_raw_transaction(idx)
		if rawhex != expected_unsigned_double_utxo_tx_hex {
			t.Errorf("Transaction state does not match expected hash.")
		}
		W_clear_transaction(idx)
	})

	t.Run("add_utxo_bad_index", func(t *testing.T) {
		idx := W_start_transaction()
		res := W_add_utxo(idx+1, hash_2_doge, vout_2_doge)
		if res != 0 {
			t.Errorf("Bad index should return failure.")
		}
		W_clear_transaction(idx)
	})

	t.Run("add_output", func(t *testing.T) {
		idx := W_store_raw_transaction(expected_unsigned_double_utxo_tx_hex)
		if idx == 0 {
			t.Errorf("Error while deserializing expected transaction.")
		}
		if W_add_output(idx, external_p2pkh_addr, send_amt) == 0 {
			t.Errorf("Error while adding output.")
		}
		rawhex := W_get_raw_transaction(idx)
		if rawhex != expected_unsigned_double_utxo_single_output_tx_hex {
			t.Errorf("Transaction state does not match expected hash.")
		}
		W_clear_transaction(idx)
	})

	t.Run("finalize_transaction", func(t *testing.T) {
		idx := W_store_raw_transaction(expected_unsigned_double_utxo_single_output_tx_hex)
		if idx == 0 {
			t.Errorf("Error while deserializing expected transaction.")
		}
		rawhex := W_finalize_transaction(idx, external_p2pkh_addr, fee, total_utxo_input, p2pkh_addr)
		if rawhex != expected_unsigned_tx_hex {
			t.Errorf("Transaction state does not match expected hash.")
		}
		W_clear_transaction(idx)
	})

	t.Run("clear_transaction", func(t *testing.T) {
		idx := W_store_raw_transaction(expected_unsigned_double_utxo_single_output_tx_hex)
		if idx == 0 {
			t.Errorf("Error while deserializing expected transaction.")
		}
		W_clear_transaction(idx)
		rawhex := W_get_raw_transaction(idx)
		if rawhex != "" {
			t.Errorf("Error while erasing current transaction.")
		}
	})

	t.Run("bad_sign_raw_transaction", func(t *testing.T) {
		idx := W_store_raw_transaction(expected_unsigned_tx_hex)
		if idx == 0 {
			t.Errorf("Error while deserializing expected transaction.")
		}
		rawhex := W_get_raw_transaction(idx)
		rawhex = W_sign_raw_transaction(0, rawhex, utxo_scriptpubkey, 1, bad_privkey_wif)
		if rawhex != "" {
			t.Errorf("Bad private key should yield empty transaction.")
		}
		W_clear_transaction(idx)
	})

	t.Run("sign_raw_transaction", func(t *testing.T) {
		idx := W_store_raw_transaction(expected_unsigned_tx_hex)
		if idx == 0 {
			t.Errorf("Error while deserializing expected transaction.")
		}
		rawhex := W_get_raw_transaction(idx)
		rawhex = W_sign_raw_transaction(0, rawhex, utxo_scriptpubkey, 1, privkey_wif)
		if rawhex != expected_signed_single_input_tx_hex {
			t.Errorf("Error signing first input.")
		}
		rawhex = W_sign_raw_transaction(1, rawhex, utxo_scriptpubkey, 1, privkey_wif)
		if rawhex != expected_signed_raw_tx_hex {
			t.Errorf("Error signing second input.")
		}
		W_clear_transaction(idx)
	})

	t.Run("sign_transaction", func(t *testing.T) {
		idx := W_store_raw_transaction(expected_unsigned_tx_hex)
		if idx == 0 {
			t.Errorf("Error while deserializing expected transaction.")
		}
		if W_sign_transaction(idx, utxo_scriptpubkey, privkey_wif) == 0 {
			t.Errorf("Error signing transaction.")
		}
		if W_get_raw_transaction(idx) != expected_signed_raw_tx_hex {
			t.Errorf("Transaction state does not match expected hash.")
		}
		W_clear_transaction(idx)
	})

	t.Run("full_int_amt_transaction_build", func(t *testing.T) {
		idx := W_start_transaction()
		W_add_utxo(idx, hash_2_doge, vout_2_doge)
		if W_get_raw_transaction(idx) != expected_unsigned_single_utxo_tx_hex {
			t.Errorf("Returned hex does not match expected hex after adding the first utxo.")
		}
		W_add_utxo(idx, hash_10_doge, vout_10_doge)
		if W_get_raw_transaction(idx) != expected_unsigned_double_utxo_tx_hex {
			t.Errorf("Returned hex does not match expected hex after adding the second utxo.")
		}
		W_add_output(idx, external_p2pkh_addr, send_amt)
		if W_get_raw_transaction(idx) != expected_unsigned_double_utxo_single_output_tx_hex {
			t.Errorf("Returned hex does not match expected hex after adding both utxos and an ouput.")
		}
		W_finalize_transaction(idx, external_p2pkh_addr, fee, total_utxo_input, p2pkh_addr)
		if W_get_raw_transaction(idx) != expected_unsigned_tx_hex {
			t.Errorf("Returned hex does not match expected hex after making change.")
		}
		W_sign_transaction(idx, utxo_scriptpubkey, privkey_wif)
		if W_get_raw_transaction(idx) != expected_signed_raw_tx_hex {
			t.Errorf("Returned hex does not match expected hex after signing inputs.")
		}
		W_clear_transaction(idx)
	})

	t.Run("full_decimal_amt_transaction_build", func(t *testing.T) {
		idx := W_start_transaction()
		W_add_utxo(idx, hash_decimal_doge, vout_decimal_doge)
		if W_get_raw_transaction(idx) != expected_unsigned_single_utxo_tx_hex2 {
			t.Errorf("Returned hex does not match expected hex after adding the first utxo.")
		}
		W_add_output(idx, external_p2pkh_addr, decimal_send_amt)
		if W_get_raw_transaction(idx) != expected_unsigned_single_utxo_single_output_tx_hex2 {
			t.Errorf("Returned hex does not match expected hex after adding both utxos and an ouput.")
		}
		W_finalize_transaction(idx, external_p2pkh_addr, fee, decimal_total_utxo_input, p2pkh_addr2)
		if W_get_raw_transaction(idx) != expected_unsigned_tx_hex2 {
			t.Errorf("Returned hex does not match expected hex after making change.")
		}
		W_sign_transaction(idx, utxo_scriptpubkey2, privkey_wif2)
		if W_get_raw_transaction(idx) != expected_signed_raw_tx_hex2 {
			t.Errorf("Returned hex does not match expected hex after signing inputs.")
		}
		W_clear_transaction(idx)
	})
}
