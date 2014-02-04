
public class LazyValue {
	private def value
	private def closure

	public LazyValue(closure) {
		this.closure = closure
	}

	public def getValue() {
		if (this.closure != null) {
			this.value = this.closure()
			this.closure = null
		}

		return this.value
	}

	public def call() {
		return getValue()
	}

	@Override
	public String toString() {
		return this.getValue()
	}
}
