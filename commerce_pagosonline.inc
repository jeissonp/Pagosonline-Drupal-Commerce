<?php
ini_set("display_errors", 1);
error_reporting(E_ERROR | E_WARNING | E_PARSE);
function commerce_pagosonline_response($order_id, $sale_reference, $option) {
  $param = commerce_pagosonline_get_param($_GET);
  if ($option == md5($order_id . '~' . 'RESPONSE' . '~'. $sale_reference . '~' . $param['extra1'])) {
    $payment_method = commerce_payment_method_instance_load($param['extra1']);
    $encryption_key = $payment_method['settings']['pagosonline_encryption_key'];
    $user_id = $payment_method['settings']['pagosonline_uid'];
    if ($param['firma'] == strtoupper(commerce_pagosonline_get_firm($encryption_key, $user_id, $sale_reference, $param['valor'], $param['moneda'], $param['estado_pol']))) {
      $order = commerce_order_load($order_id);
      if ($order) {
        $result_transation = commerce_pagosonline_save_transation($param, $order);
        commerce_order_save($order);
        switch ($result_transation) {
          case COMMERCE_PAYMENT_STATUS_FAILURE:
            drupal_set_message(t('Order Canceled'), 'error');
            break;
          default:
            drupal_set_message(t('Payment pending confirmation'));
            break;
        }
        return '';
      }
      else {
        drupal_set_message(t('Order not found'), 'error');
      }
    }
    else {
      drupal_set_message(t('Error of Parameters'), 'error');
    }
  }
  else {
    drupal_access_denied();
  }
}

function commerce_pagosonline_confirmation($order_id, $sale_reference, $option) {
  $param = commerce_pagosonline_get_param($_POST);
  
  if ($option == md5($order_id . '~' . 'CONFIRMATION' . '~'. $sale_reference . '~' . $param['extra1'])) {

    $payment_method = commerce_payment_method_instance_load($param['extra1']);
    $encryption_key = $payment_method['settings']['pagosonline_encryption_key'];
    $user_id = $payment_method['settings']['pagosonline_uid'];
  
    if ($param['firma'] == drupal_strtoupper(commerce_pagosonline_get_firm($encryption_key, $user_id, $sale_reference, $param['valor'], $param['moneda'], $param['estado_pol']))) {
      
      $order = commerce_order_load($order_id);
      
      if ($order) {
        
        $result_transation = commerce_pagosonline_save_transation($param, $order, 'CONFIRMATION');
        commerce_order_save($order);
        
        if ($result_transation != COMMERCE_PAYMENT_STATUS_FAILURE) {
          commerce_checkout_complete($order);
        }
        module_invoke_all('pagosonline_confirmation', $order, $transaction);
      }
      else {
          
        watchdog('pagosonline', 'Order not found');
        
        //drupal_set_message(t('Order not found'), 'error');
      }
    }
    else {
      watchdog('pagosonline', 'Error of Parameters');
      //drupal_set_message(t('Error of Parameters'), 'error');
    }
  }
  else {
    watchdog('pagosonline', 'Access Denied');
    //drupal_access_denied();
  }
}
function commerce_pagosonline_save_transation($param, $order, $method = 'RESPONSE') {
  //dpm($param);
  if ($param['cus'] == 'null') {
    $param['cus'] = 0;
  }
  
  //try {

    $result = db_insert('pagosonline_log')
      ->fields(array(
        'order_id' => $order->order_id,
        'created' => REQUEST_TIME,
        'state_transation' => $param['estado_pol'],
        'risk' => $param['riesgo'],
        'code_response' => $param['codigo_respuesta_pol'],
        'sale_reference' => $param['ref_venta'],
        'reference_pagosonline' => $param['ref_pol'],
        'signature' => $param['firma'],
        'extra1' => $param['extra1'],
        'extra2' => $param['extra2'],
        'method_payment' => $param['medio_pago'],
        'type_method_payment' => $param['tipo_medio_pago'],
        'number_payment' => isset($param['cuotas']) ? $param['cuotas'] : 0,
        'value' => $param['valor'],
        'tax' => $param['iva'],
        'value_aditional' => $param['valorAdicional'],
        'currency' => $param['moneda'],
        'date_transation' => isset($param['fecha_transaccion']) ? $param['fecha_transaccion'] : '',
        'authorization_code' => isset($param['codigo_autorizacion']) ? $param['codigo_autorizacion'] : '',
        'cus' => $param['cus'],
        'bank_pse' => $param['banco_pse'],
        'email_buyer' => isset($param['email_comprador']) ? $param['email_comprador'] : ''))
      ->execute();
  //} catch (Exception $e) {
  //    dpm($e->getMessage());
  //}

  $path_transation = 'pagosonline/transaction/view/' . $result . '/' . md5($result . '~' . 'VIEW-TRANSATION');
  $transaction = commerce_payment_transaction_new('pagosonline', $order->order_id);
  
  $message = array();
  
  $message[] = ' - - - - - -  <b>METHOD ' . $method . '</b> - - - - - -';
  $message[] = $param['estado_pol'] . ': ' . commerce_pagosonline_transaction_state($param['estado_pol']);
  $message[] = $param['codigo_respuesta_pol'] . ': ' . commerce_pagosonline_response_code($param['codigo_respuesta_pol']);
  $message[] = l(t('View transaction'), $path_transation);

  switch ($param['estado_pol']) {
    case 4:// Payment Received
      $transaction->status = COMMERCE_PAYMENT_STATUS_SUCCESS;
      $order->status = 'checkout_complete';
      break;          
    case 7: // Pending or Processing
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
      $transaction->status = COMMERCE_PAYMENT_STATUS_PENDING;
      $order->status = 'checkout_review';
      break;
    default:
      $transaction->status = COMMERCE_PAYMENT_STATUS_FAILURE;
      $order->status = 'canceled';
  }
  
  $transaction->instance_id = $param['extra1'];
  $transaction->amount = $param['valor'];
  $transaction->currency_code = $param['moneda'];
  $transaction->message = implode('<br />', $message);
  commerce_payment_transaction_save($transaction);
  return $transaction->status;
}

/*
 * Load parameters 
 **/
function commerce_pagosonline_get_param($target) {
  $param = array();
  if (is_array($target)) {
    foreach ($target as $key => $value) {
      $param[$key] = check_plain($_REQUEST[$key]);
    }
  }
  return $param;
}

function commerce_pagosonline_response_code($key = NULL) {
  $code = array(
    1     => t('Transaction approved'), // 'Transacción aprobada',
    2     => t('Payment canceled by the user'), // 'Pago cancelado por el usuario',
    3     => t('Payment canceled by the user while validating'), // 'Pago cancelado por el usuario durante validación',
    4     => t('Transaction rejected by the entity'), // 'Transacción rechazada por la entidad',
    5     => t('Transaction declined by the entity'), // 'Transacción declinada por la entidad',
    6     => t('Insufficient funds'), // 'Fondos insuficientes',
    7     => t('Invalid card'), // 'Tarjeta invalida',
    8     => t('Go to your bank'), // 'Acuda a su entidad',
    9     => t('Expired card'), // 'Tarjeta vencida',
    10    => t('Restricted card'), // 'Tarjeta restringida',
    11    => t('POL Discretionary'), // 'Discrecional POL',
    12    => t('Expiration date or security code invalid'), // 'Fecha de expiración o campo seg. Inválidos',
    13    => t('Repit transaction'), // 'Repita transacción',
    14    => t('Invalid transaction'), // 'Transacción inválida',
    15    => t('Transaction in validating process'), // 'Transacción en proceso de validación',
    16    => t('Invalid user-password combination'), // 'Combinación usuario-contraseña inválidos',
    17    => t('Amount exceeds maximum allowed by the entity'), // 'Monto excede máximo permitido por entidad',
    18    => t('Invalid identification document'), // 'Documento de identificación inválido',
    19    => t('Paid Transaction, capturing data'), // 'Transacción abandonada capturando datos TC',
    20    => t('Abandoned transaction'), // 'Transacción abandonada',
    21    => t('Impossible to reverse transaction'), // 'Imposible reversar transacción',
    22    => t('Card not allowed to do payments by internet'), // 'Tarjeta no autorizada para realizar compras por internet.',
    23    => t('Rejected transaction'), // 'Transacción rechazada',
    24    => t('Partial transaction approved'), // 'Transacción parcial aprobada',
    25    => t('Rejected no-confirmation'), // 'Rechazada por no confirmación',
    26    => t('Voucher generated, waiting for bank payment'), // 'Comprobante generado, esperando pago en banco',
    9994  => t('Transaction pending to be confirmed'), // 'Transacción pendiente por confirmar',
    9995  => t('Digital certificate not found'), // 'Certificado digital no encontrado',
    9996  => t("Entity doesn't answer"), // 'Entidad no responde',
    9997  => t('Messaging error with the bank'), // 'Error de mensajería con la entidad financiera',
    9998  => t('Error in bank'), // 'Error en la entidad financiera',
    9999  => t('Not specified error'), // 'Error no especificado',
  );
  return $key ? $code[$key] : $code;
}

function commerce_pagosonline_transaction_state($key = NULL){
  $state = array(
    1   => t('Unopened'), // 'Sin abrir',
    2   => t('Opened'), // 'Abierta',
    4   => t('Paid'), // 'Pagada y abonada',
    5   => t('Canceled'), // 'Cancelada',
    6   => t('Rejected'), // 'Rechazada',
    7   => t('Validating'), // 'En validación',
    8   => t('Reversed'), // 'Reversada',
    9   => t('Fraudulent reversed'), // 'Reversada fraudulenta',
    10  => t('Sent to financial institution'), // 'Enviada ent. Financiera',
    11  => t('Capturing credit card data'), // 'Capturando datos tarjeta de crédito',
    12  => t('Awaiting PSE system confirmation'), // 'Esperando confirmación sistema PSE',
    13  => t('Active ACH Debits'), // 'Activa Débitos ACH',
    14  => t('Confirming Efecty payment'), // 'Confirmando pago Efecty',
    15  => t('Printed'), // 'Impreso',
    16  => t('ACH debit registered'), // 'Debito ACH Registrado',
  );
  return $key ? $state[$key] : $state;
}

function commerce_pagosonline_language($key = NULL) {
  $languages = array(
    'fr' => t('French'),
    'en' => t('English'),
    'es' => t('Spanish'),
    'it' => t('Italian'),
  ); 
  return $key ? $languages[$key] : $languages;
}

function commerce_pagosonline_currencies($key = NULL) {
  $currencies = array(
    'COP' => t('Colombian peso'),
    'EUR' => t('Euro'),
    'GBP' => t('Pound sterling'),
    'MXN' => t('Mexican peso'),
    'USD' => t('United States dollar'),
    'VEB' => t('Venezuelan bolivar fuerte'),
  );
  return $key ? $currencies[$key] : $currencies;
}

function commerce_pagosonline_state_transation($key = NULL) {
  $state_transation = array(
    1   => t('Unopened'), // 'Sin abrir',
    2   => t('Opened'), // 'Abierta',
    4   => t('Paid'), // 'Pagada y abonada',
    5   => t('Canceled'), // 'Cancelada',
    6   => t('Rejected'), // 'Rechazada',
    7   => t('Validating'), // 'En validación',
    8   => t('Reversed'), // 'Reversada',
    9   => t('Fraudulent reversed'), // 'Reversada fraudulenta',
    10  => t('Sent to financial institution'), // 'Enviada ent. Financiera',
    11  => t('Capturing credit card data'), // 'Capturando datos tarjeta de crédito',
    12  => t('Awaiting PSE system confirmation'), // 'Esperando confirmación sistema PSE',
    13  => t('Active ACH Debits'), // 'Activa Débitos ACH',
    14  => t('Confirming Efecty payment'), // 'Confirmando pago Efecty',
    15  => t('Printed'), // 'Impreso',
    16  => t('ACH debit registered'), // 'Debito ACH Registrado',
  );
  return $key ? $state_transation[$key] : $state_transation;
}

function commerce_pagosonline_payment_form($key = NULL) {
  $payment_form = array(
    10 => t('VISA'), // 'VISA',
    11 => t('MASTERCARD'), // 'MASTERCARD',
    12 => t('AMEX'), // 'AMEX',
    22 => t('DINERS'), // 'DINERS',
    24 => t('Verified by VISA'), // 'Verified by VISA',
    25 => t('PSE'), // 'PSE',
    27 => t('VISA debit'), // 'VISA Debito',
    30 => t('Efecty'), // 'Efecty',
    31 => t('Referenced payment'), // 'Pago referenciado',
  );
  return $key ? $payment_form[$key] : $payment_form;
}

/*
 * Digital signature of the transaction
 */
function commerce_pagosonline_get_firm($encryption_key, $user_id, $sale_reference, $value, $currency_type, $state_transation = "") {  
  if (empty($state_transation)) { //Format firm digital sending: "llaveEncripcion~usuario_id~ref_venta~valor~moneda"
    return md5($encryption_key . '~' . $user_id . '~' . $sale_reference . '~' . $value . '~' . $currency_type);
  }
  else { //Format firm digital sending: "llaveEncripcion~usuario_id~ref_venta~valor~moneda~estado_pol"
    return md5($encryption_key . '~' . $user_id . '~' . $sale_reference . '~' . $value . '~' . $currency_type .  '~' . $state_transation);
  }
}

/*
 * Generate number reference unique
 */
function commerce_pagosonline_get_reference($order) {    
  return 'commerce-' . $order->order_id;
}
