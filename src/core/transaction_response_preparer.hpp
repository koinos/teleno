#pragma once

namespace koinos::rpc::chain {
class submit_transaction_response;
}

namespace koinos::node {

// Synchronous, borrowed callback. Native transaction execution invokes it once
// after building the receipt and before publishing accepted work. A local
// response-capacity failure can therefore refuse admission without undoing an
// accepted transaction. No callback is installed for ordinary native callers.
struct TransactionResponsePreparer
{
  void* context = nullptr;
  void ( *prepare )( void*, const rpc::chain::submit_transaction_response& ) = nullptr;

  explicit operator bool() const noexcept { return prepare != nullptr; }
  void operator()( const rpc::chain::submit_transaction_response& response ) const
  {
    if( prepare ) prepare( context, response );
  }
};

} // namespace koinos::node
