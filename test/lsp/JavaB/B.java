public class B {
	private static class Something {
		public int x = 0;
		int f() {
			return x;
		}
	}
	
	public static void main(String[] args) {
		Something s = new Something();
		s.f();
	}
}
